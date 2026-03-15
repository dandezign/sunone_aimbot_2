#include "sam3.cuh"
#include "sam3.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <thread>

namespace {
constexpr int kPromptLength = 32;
constexpr int64_t kPadToken = 49407;
constexpr int64_t kBosToken = 49406;

#ifndef SAM3_DEFAULT_TOKENIZER_SCRIPT
#define SAM3_DEFAULT_TOKENIZER_SCRIPT "SAM3TensorRT/python/tokenize_prompt.py"
#endif

void set_person_prompt(std::vector<int64_t> &ids, std::vector<int64_t> &mask) {
  ids = {49406, 2533,  49407, 49407, 49407, 49407, 49407, 49407,
         49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407,
         49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407,
         49407, 49407, 49407, 49407, 49407, 49407, 49407, 49407};

  mask = {1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
          0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}

bool parse_ids_csv(const std::string &csv, std::vector<int64_t> &parsed) {
  std::stringstream ss(csv);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (item.empty()) {
      continue;
    }

    size_t processed = 0;
    long long value = 0;
    try {
      value = std::stoll(item, &processed);
    } catch (const std::exception &) {
      return false;
    }

    if (processed != item.size()) {
      return false;
    }

    parsed.push_back(static_cast<int64_t>(value));
  }

  return !parsed.empty();
}

std::string trim_copy(const std::string &s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

std::string shell_quote(const std::string &s) {
  std::string out = "\"";
  for (char ch : s) {
    if (ch == '"') {
      out += "\\\"";
    } else {
      out += ch;
    }
  }
  out += "\"";
  return out;
}

std::string normalize_windows_path(std::string s) {
#ifdef _WIN32
  std::replace(s.begin(), s.end(), '/', '\\');
#endif
  return s;
}

bool parse_mask_csv(const std::string &csv, std::vector<int64_t> &mask) {
  std::vector<int64_t> parsed;
  if (!parse_ids_csv(csv, parsed)) {
    return false;
  }

  for (int64_t v : parsed) {
    if (v != 0 && v != 1) {
      return false;
    }
  }

  mask = std::move(parsed);
  return true;
}

bool run_command_capture(const std::string &command, std::string &output) {
#ifdef _WIN32
  FILE *pipe = _popen(command.c_str(), "r");
#else
  FILE *pipe = popen(command.c_str(), "r");
#endif
  if (!pipe) {
    return false;
  }

  char buffer[512];
  output.clear();
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }

#ifdef _WIN32
  const int code = _pclose(pipe);
#else
  const int code = pclose(pipe);
#endif

  return code == 0;
}

bool tokenize_prompt_with_python(const std::string &prompt,
                                 std::vector<int64_t> &ids,
                                 std::vector<int64_t> &mask) {
  const char *python_env = std::getenv("SAM3_TOKENIZER_PYTHON");
  const char *script_env = std::getenv("SAM3_TOKENIZER_SCRIPT");

  const std::string python_cmd =
      (python_env && *python_env) ? std::string(python_env) : "python";
  const std::string script_path =
      (script_env && *script_env) ? std::string(script_env)
                                  : std::string(SAM3_DEFAULT_TOKENIZER_SCRIPT);

  const std::string python_cmd_norm = normalize_windows_path(python_cmd);
  const std::string script_path_norm = normalize_windows_path(script_path);

#ifdef _WIN32
  const std::string command = python_cmd_norm + " " + script_path_norm +
                              " --text " + shell_quote(prompt) +
                              " --max-length " + std::to_string(kPromptLength);
#else
  const std::string command = shell_quote(python_cmd_norm) + " " +
                              shell_quote(script_path_norm) + " --text " +
                              shell_quote(prompt) + " --max-length " +
                              std::to_string(kPromptLength);
#endif

  std::string output;
  if (!run_command_capture(command, output)) {
    return false;
  }

  std::stringstream ss(output);
  std::string line;
  std::string ids_line;
  std::string mask_line;

  while (std::getline(ss, line)) {
    line = trim_copy(line);
    if (line.rfind("IDS:", 0) == 0) {
      ids_line = line.substr(4);
    } else if (line.rfind("MASK:", 0) == 0) {
      mask_line = line.substr(5);
    }
  }

  std::vector<int64_t> parsed_ids;
  std::vector<int64_t> parsed_mask;
  if (!parse_ids_csv(ids_line, parsed_ids) ||
      !parse_mask_csv(mask_line, parsed_mask)) {
    return false;
  }

  if (parsed_ids.size() != static_cast<size_t>(kPromptLength) ||
      parsed_mask.size() != static_cast<size_t>(kPromptLength)) {
    return false;
  }

  ids = std::move(parsed_ids);
  mask = std::move(parsed_mask);
  return true;
}

bool build_prompt_from_arg(const std::string &arg, std::vector<int64_t> &ids,
                           std::vector<int64_t> &mask) {
  if (arg.empty() || arg == "person") {
    set_person_prompt(ids, mask);
    return true;
  }

  std::string raw = arg;
  if (raw.rfind("prompt=", 0) == 0) {
    raw = raw.substr(7);
  }

  if (raw == "person") {
    set_person_prompt(ids, mask);
    return true;
  }

  if (raw.rfind("ids:", 0) == 0) {
    std::vector<int64_t> tokens;
    if (!parse_ids_csv(raw.substr(4), tokens)) {
      return false;
    }

    if (tokens.size() >= static_cast<size_t>(kPromptLength)) {
      return false;
    }

    if (tokens.front() != kBosToken) {
      tokens.insert(tokens.begin(), kBosToken);
    }

    if (tokens.size() > static_cast<size_t>(kPromptLength)) {
      return false;
    }

    ids.assign(kPromptLength, kPadToken);
    mask.assign(kPromptLength, 0);

    for (size_t i = 0; i < tokens.size(); ++i) {
      ids[i] = tokens[i];
      mask[i] = 1;
    }

    return true;
  }

  if (raw.rfind("prompt=", 0) == 0) {
    raw = raw.substr(7);
  }

  if (!raw.empty()) {
    return tokenize_prompt_with_python(raw, ids, mask);
  }

  return false;
}
} // namespace

void infer_one_image(SAM3_PCS &pcs, const cv::Mat &img, cv::Mat &result,
                     const SAM3_VISUALIZATION vis, const std::string outfile,
                     bool benchmark_run) {
  bool success = pcs.infer_on_image(img, result, vis);

  if (benchmark_run) {
    return;
  }

  if (vis == SAM3_VISUALIZATION::VIS_NONE) {
    cv::Mat seg = cv::Mat(SAM3_OUTMASK_WIDTH, SAM3_OUTMASK_HEIGHT, CV_32FC1,
                          pcs.output_cpu[1]);
    // these are raw logits and should be passed through sigmoid before for any
    // quantitative use
  } else {
    cv::imwrite(outfile, result);
  }
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cout << "Usage: ./sam3_pcs_app indir engine_path.engine "
                 "[benchmark=0|1] [prompt=person|ids:<csv>]"
              << std::endl;
    return 0;
  }

  const std::string in_dir = argv[1];
  std::string epath = argv[2];
  bool benchmark = false; // in benchmarking mode we dont save output images
  std::string prompt_arg = "person";

  for (int argi = 3; argi < argc; ++argi) {
    std::string arg = argv[argi];

    if (arg == "0" || arg == "1") {
      benchmark = (arg == "1");
      continue;
    }

    if (arg.rfind("prompt=", 0) == 0 || arg.rfind("ids:", 0) == 0 ||
        arg == "person") {
      prompt_arg = arg;
      continue;
    }

    std::cout << "Ignoring unrecognized argument: " << arg << std::endl;
  }

  std::cout << "Benchmarking: " << benchmark << std::endl;

  auto start = std::chrono::system_clock::now();
  auto end = std::chrono::system_clock::now();
  std::chrono::duration<float> diff;
  float millis_elapsed = 0.0; // int will overflow after ~650 hours

  const float vis_alpha = 0.3;
  const float probability_threshold = 0.5;
  const SAM3_VISUALIZATION visualize =
      SAM3_VISUALIZATION::VIS_SEMANTIC_SEGMENTATION;

  SAM3_PCS pcs(epath, vis_alpha, probability_threshold);

  cv::Mat img, result;
  cv::Size pinned_size;

  std::filesystem::create_directories("results");
  int num_images_read = 0;

  std::vector<int64_t> iid;
  std::vector<int64_t> iam;

  if (!build_prompt_from_arg(prompt_arg, iid, iam)) {
    std::cerr << "Unsupported prompt argument: " << prompt_arg << std::endl;
    std::cerr << "Supported values: prompt=<any text> or ids:<csv_of_token_ids>"
              << std::endl;
    std::cerr << "Hint: set SAM3_TOKENIZER_PYTHON and SAM3_TOKENIZER_SCRIPT if"
                 " tokenizer helper cannot be launched."
              << std::endl;
    return 1;
  }

  pcs.set_prompt(iid, iam);

  for (const auto &fname : std::filesystem::directory_iterator(in_dir)) {
    if (std::filesystem::is_regular_file(fname.path())) {
      std::filesystem::path outfile =
          std::filesystem::path("results") / fname.path().filename();
      cv::Mat decoded = cv::imread(fname.path().string(), cv::IMREAD_COLOR);

      if (decoded.empty()) {
        std::cout << "Skipping unreadable image: " << fname.path() << std::endl;
        continue;
      }

      if (num_images_read == 0) {
        img = decoded.clone();
        result = decoded.clone();
        pinned_size = img.size();
        pcs.pin_opencv_matrices(img, result);
      } else {
        if (decoded.size() != pinned_size) {
          cv::resize(decoded, img, pinned_size, 0.0, 0.0, cv::INTER_LINEAR);
        } else {
          decoded.copyTo(img);
        }
      }
      start = std::chrono::system_clock::now();
      infer_one_image(pcs, img, result, visualize, outfile.string(), benchmark);
      num_images_read++;
      end = std::chrono::system_clock::now();
      diff = end - start;
      millis_elapsed += (diff.count() * 1000);

      if (num_images_read > 0 && num_images_read % 10 == 0) {
        float msec_per_image = millis_elapsed / num_images_read;
        printf("Processed %d images at %f msec/image\n", num_images_read,
               msec_per_image);
      }
    }
  }
}
