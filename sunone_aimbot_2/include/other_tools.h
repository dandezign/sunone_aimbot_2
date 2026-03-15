#ifndef OTHER_TOOLS_H
#define OTHER_TOOLS_H

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <vector>
#include <d3d11.h>

namespace OtherTools
{
inline int MaxInt(int a, int b) noexcept
{
    return (a > b) ? a : b;
}

inline float MaxFloat(float a, float b) noexcept
{
    return (a > b) ? a : b;
}
}

std::vector<unsigned char> Base64Decode(const std::string& encoded_string);
bool fileExists(const std::string& path);
std::string replace_extension(const std::string& filename, const std::string& new_extension);

inline std::filesystem::path GetCurrentExecutablePath()
{
    wchar_t exePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (length == 0 || length == MAX_PATH)
    {
        return {};
    }

    return std::filesystem::path(exePath);
}

inline std::filesystem::path GetExecutableModelsDirectory()
{
    const std::filesystem::path executablePath = GetCurrentExecutablePath();
    if (executablePath.empty())
    {
        return {};
    }

    return executablePath.parent_path() / "models";
}

inline bool HasCaseInsensitiveStemPrefix(const std::filesystem::path& path, const std::string& prefix)
{
    std::string stem = path.stem().string();
    std::transform(stem.begin(), stem.end(), stem.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    std::string normalizedPrefix = prefix;
    std::transform(normalizedPrefix.begin(), normalizedPrefix.end(), normalizedPrefix.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    return stem.rfind(normalizedPrefix, 0) == 0;
}

inline std::string GetLowercaseModelExtension(const std::filesystem::path& path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

inline bool ShouldIncludeCoreAiModelEntry(const std::string& fileName)
{
    const std::filesystem::path path(fileName);
    const std::string extension = GetLowercaseModelExtension(path);
    if (extension != ".engine" && extension != ".onnx")
    {
        return false;
    }

    return !HasCaseInsensitiveStemPrefix(path, "sam3");
}

inline bool ShouldIncludeSam3EngineEntry(const std::string& fileName)
{
    const std::filesystem::path path(fileName);
    return GetLowercaseModelExtension(path) == ".engine"
        && HasCaseInsensitiveStemPrefix(path, "sam3");
}

std::vector<std::string> getEngineFiles();
std::vector<std::string> getModelFiles();
std::vector<std::string> getOnnxFiles();
std::vector<std::string> getAvailableCoreAiModels();
std::vector<std::string> getAvailableSam3Engines();
std::vector<std::string>::difference_type getModelIndex(const std::vector<std::string>& engine_models);
std::vector<std::string> getAvailableDepthModels();

bool LoadTextureFromFile(const char* filename, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
bool LoadTextureFromMemory(const std::string& imageBase64, ID3D11Device* device, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);

std::string get_ghub_version();
int get_active_monitors();
HMONITOR GetMonitorHandleByIndex(int monitorIndex);
void SetRandomConsoleTitle();
bool IsValidImageFile(const std::wstring& wpath, UINT& outW, UINT& outH, std::string& outErr);

std::vector<std::string> getAvailableModels();

void welcome_message();
bool checkwin1903();
std::string WideToUtf8(const std::wstring& ws);
#endif // OTHER_TOOLS_H
