#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <windows.h>
#include <commdlg.h>

using namespace std;

vector<double> timeData;

struct TemperatureChannel {
    string key;
    string label;
};

const vector<TemperatureChannel> temperatureChannels = {
    {"TempR2", "Room 2 Temp"},
    {"TempR3", "Room 3 Temp"},
    {"TempR4", "Room 4 Temp"},
    {"TempR5LF", "Room 5 Left Front Temp"},
    {"TempR5LM", "Room 5 Left Middle Temp"},
    {"TempR5LB", "Room 5 Left Back Temp"},
    {"TempR5RF", "Room 5 Right Front Temp"},
    {"TempR5RM", "Room 5 Right Middle Temp"},
    {"TempR5RB", "Room 5 Right Back Temp"}
};

vector<vector<double>> temperatureData(temperatureChannels.size());

struct PlotSeries {
    vector<double> timestamps;
    vector<double> temperatures;
};

struct PlotDataCache {
    double xMin = numeric_limits<double>::quiet_NaN();
    double xMax = numeric_limits<double>::quiet_NaN();
    int pixelWidth = 0;
    uint64_t dataRevision = numeric_limits<uint64_t>::max();
    vector<PlotSeries> series = vector<PlotSeries>(temperatureChannels.size());
};

struct TemperatureEntry {
    double timestamp;
    vector<double> temperatures;
};

void BuildPlotSeries(
    const vector<double>& timestamps,
    const vector<double>& temperatures,
    double xMin,
    double xMax,
    int pixelWidth,
    PlotSeries& output) {
    output.timestamps.clear();
    output.temperatures.clear();
    if (timestamps.empty() || temperatures.size() != timestamps.size()) {
        return;
    }

    size_t first = static_cast<size_t>(
        lower_bound(timestamps.begin(), timestamps.end(), xMin) - timestamps.begin());
    size_t last = static_cast<size_t>(
        upper_bound(timestamps.begin(), timestamps.end(), xMax) - timestamps.begin());
    if (first > 0) {
        --first;
    }
    if (last < timestamps.size()) {
        ++last;
    }
    if (first >= last) {
        return;
    }

    const size_t visibleCount = last - first;
    const size_t bucketCount = static_cast<size_t>(max(pixelWidth, 1));
    if (visibleCount <= bucketCount * 2) {
        output.timestamps.assign(timestamps.begin() + first, timestamps.begin() + last);
        output.temperatures.assign(temperatures.begin() + first, temperatures.begin() + last);
        return;
    }

    const double range = xMax - xMin;
    if (!isfinite(range) || range <= 0.0) {
        output.timestamps.assign(timestamps.begin() + first, timestamps.begin() + last);
        output.temperatures.assign(temperatures.begin() + first, temperatures.begin() + last);
        return;
    }

    output.timestamps.reserve(bucketCount * 2);
    output.temperatures.reserve(bucketCount * 2);
    size_t index = first;
    while (index < last) {
        const auto bucketFor = [xMin, range, bucketCount](double timestamp) {
            const double position = (timestamp - xMin) / range * bucketCount;
            if (position <= 0.0) {
                return size_t{0};
            }
            if (position >= static_cast<double>(bucketCount)) {
                return bucketCount - 1;
            }
            return static_cast<size_t>(position);
        };
        const size_t bucket = bucketFor(timestamps[index]);
        size_t bucketEnd = index + 1;
        while (bucketEnd < last && bucketFor(timestamps[bucketEnd]) == bucket) {
            ++bucketEnd;
        }

        size_t runStart = index;
        while (runStart < bucketEnd) {
            if (!isfinite(temperatures[runStart])) {
                output.timestamps.push_back(timestamps[runStart]);
                output.temperatures.push_back(numeric_limits<double>::quiet_NaN());
                do {
                    ++runStart;
                } while (runStart < bucketEnd && !isfinite(temperatures[runStart]));
                continue;
            }

            size_t runEnd = runStart;
            size_t minIndex = runStart;
            size_t maxIndex = runStart;
            while (runEnd + 1 < bucketEnd && isfinite(temperatures[runEnd + 1])) {
                ++runEnd;
                if (temperatures[runEnd] < temperatures[minIndex]) {
                    minIndex = runEnd;
                }
                if (temperatures[runEnd] > temperatures[maxIndex]) {
                    maxIndex = runEnd;
                }
            }

            size_t selected[4] = {runStart, minIndex, maxIndex, runEnd};
            sort(begin(selected), end(selected));
            size_t previous = numeric_limits<size_t>::max();
            for (size_t selectedIndex : selected) {
                if (selectedIndex != previous) {
                    output.timestamps.push_back(timestamps[selectedIndex]);
                    output.temperatures.push_back(temperatures[selectedIndex]);
                    previous = selectedIndex;
                }
            }
            runStart = runEnd + 1;
        }
        index = bucketEnd;
    }
}

double DateStringToEpoch(const string& dateStr) {
    tm t = {};
    stringstream ss(dateStr);
    char dash, colon;

    if (ss >> t.tm_year >> dash >> t.tm_mon >> dash >> t.tm_mday
           >> t.tm_hour >> colon >> t.tm_min >> colon >> t.tm_sec)
    {
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        t.tm_isdst = -1;

        time_t epochTime = mktime(&t);
        if (epochTime != -1) {
            return static_cast<double>(epochTime);
        }
    }
    return -1.0;
}

bool ParseTemperature(const string& line, const string& key, double& temperature) {
    const string marker = "\"" + key + "\":";
    const size_t markerPos = line.find(marker);
    if (markerPos == string::npos) {
        return false;
    }

    const size_t valueStart = markerPos + marker.length();
    const size_t valueEnd = line.find_first_of(",}", valueStart);
    const string valueText = line.substr(
        valueStart,
        valueEnd == string::npos ? string::npos : valueEnd - valueStart);

    try {
        size_t parsedLength = 0;
        const double parsedValue = stod(valueText, &parsedLength);
        while (parsedLength < valueText.length() &&
               (valueText[parsedLength] == ' ' || valueText[parsedLength] == '\t' ||
                valueText[parsedLength] == '\r')) {
            ++parsedLength;
        }
        if (parsedLength != valueText.length()) {
            return false;
        }

        temperature = parsedValue / 10.0;
        return true;
    } catch (const invalid_argument&) {
        return false;
    } catch (const out_of_range&) {
        return false;
    }
}

TemperatureEntry ParseLine(const string& line) {
    TemperatureEntry entry{
        -1.0,
        vector<double>(temperatureChannels.size(), numeric_limits<double>::quiet_NaN())
    };
    if (line.empty()) return entry;

    auto getJsonString = [&line](const string& key, string& value) {
        const string marker = "\"" + key + "\":\"";
        const size_t valueStart = line.find(marker);
        if (valueStart == string::npos) return false;

        const size_t contentStart = valueStart + marker.length();
        const size_t valueEnd = line.find('"', contentStart);
        if (valueEnd == string::npos) return false;

        value = line.substr(contentStart, valueEnd - contentStart);
        return true;
    };

    string date;
    string time;
    if (getJsonString("Date", date) && getJsonString("Time", time)) {
        entry.timestamp = DateStringToEpoch(date + " " + time);
    }

    if (entry.timestamp < 0.0 && line.length() >= 19) {
        entry.timestamp = DateStringToEpoch(line.substr(0, 19));
    }

    for (size_t i = 0; i < temperatureChannels.size(); ++i) {
        ParseTemperature(line, temperatureChannels[i].key, entry.temperatures[i]);
    }

    return entry;
}

struct LogFileVersion {
    uintmax_t size;
    filesystem::file_time_type lastWriteTime;
};

bool operator==(const LogFileVersion& left, const LogFileVersion& right) {
    return left.size == right.size && left.lastWriteTime == right.lastWriteTime;
}

bool GetLogFileVersion(const string& filePath, LogFileVersion& version, string& error) {
    error_code errorCode;
    version.size = filesystem::file_size(filePath, errorCode);
    if (errorCode) {
        error = "Unable to check log file: " + errorCode.message();
        return false;
    }

    version.lastWriteTime = filesystem::last_write_time(filePath, errorCode);
    if (errorCode) {
        error = "Unable to check log file: " + errorCode.message();
        return false;
    }

    return true;
}

bool ReloadLogFile(
    const string& filePath,
    vector<double>& timestamps,
    vector<vector<double>>& temperatures,
    LogFileVersion& loadedVersion,
    string& error) {
    LogFileVersion currentVersion;
    if (!GetLogFileVersion(filePath, currentVersion, error)) {
        return false;
    }

    ifstream input(filePath);
    if (!input.is_open()) {
        error = "Unable to open log file for reading.";
        return false;
    }

    vector<double> newTimestamps;
    vector<vector<double>> newTemperatures(temperatureChannels.size());
    string line;
    while (getline(input, line)) {
        TemperatureEntry data = ParseLine(line);
        if (data.timestamp >= 0.0) {
            newTimestamps.push_back(data.timestamp);
            for (size_t i = 0; i < data.temperatures.size(); ++i) {
                newTemperatures[i].push_back(data.temperatures[i]);
            }
        }
    }

    if (input.bad()) {
        error = "An error occurred while reading the log file.";
        return false;
    }

    timestamps.swap(newTimestamps);
    temperatures.swap(newTemperatures);
    loadedVersion = currentVersion;
    error.clear();
    return true;
}

// Function to trigger the built-in Windows Open File Explorer window
string OpenFileDialog() {
    char szFile[260] = { 0 };
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        return string(ofn.lpstrFile);
    }
    return ""; // Return empty string if the user cancelled the action
}

// Helper function to get the exact directory where the .exe file sits
string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    string::size_type pos = string(buffer).find_last_of("\\/");
    return string(buffer).substr(0, pos);
}

int main() {
    // Force the file path to exist directly next to the launching executable binary (.exe)
    string filePath = GetExecutableDirectory() + "\\temperature_log.txt";

    string fileError;
    error_code fileSystemError;
    if (!filesystem::exists(filePath, fileSystemError) && !fileSystemError) {
        ofstream createFile(filePath);
        if (createFile.is_open()) {
            createFile << "2026-02-04 12:00:00,\"TempR2\":200.0,\"TempR3\":210.0,\n";
        } else {
            fileError = "Unable to create the default log file.";
        }
    } else if (fileSystemError) {
        fileError = "Unable to check the default log file: " + fileSystemError.message();
    }

    if (!glfwInit()) return -1;
    
    GLFWwindow* window = glfwCreateWindow(1024, 768, "Live Temperature Telemetry Core", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImPlot::GetStyle().UseLocalTime = true;
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    const auto fileCheckInterval = chrono::seconds(2);
    auto lastFileCheck = chrono::steady_clock::now() - fileCheckInterval;
    LogFileVersion loadedVersion{};
    bool hasLoadedFile = false;
    uint64_t dataRevision = 0;
    PlotDataCache plotDataCache;
    bool fitPlotToData = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Rescan the full file when its size or last-write time changes.
        const auto now = chrono::steady_clock::now();
        const bool shouldCheckFile = now - lastFileCheck >= fileCheckInterval;
        if (shouldCheckFile) {
            lastFileCheck = now;
        }
        if (shouldCheckFile && !filePath.empty()) {
            LogFileVersion currentVersion{};
            if (!GetLogFileVersion(filePath, currentVersion, fileError)) {
                hasLoadedFile = false;
            } else if (!hasLoadedFile || !(currentVersion == loadedVersion)) {
                if (ReloadLogFile(filePath, timeData, temperatureData, loadedVersion, fileError)) {
                    hasLoadedFile = true;
                    ++dataRevision;
                }
            } else {
                fileError.clear();
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
        ImGui::Begin("Dashboard Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        ImGui::Text("System Telemetry Workspace");
        ImGui::Text("Active Log Location: %s", filePath.c_str());
        ImGui::Text("Total Live Graph Points: %llu", timeData.size());
        if (!fileError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Log file error: %s", fileError.c_str());
        }
        ImGui::Separator();

        // 2. FILE SELECTION INTERFACE AREA
        if (ImGui::Button("Browse Log File...", ImVec2(200, 30))) {
            string selectedFile = OpenFileDialog();
            if (!selectedFile.empty()) {
                filePath = selectedFile;
                
                timeData.clear();
                for (vector<double>& channelData : temperatureData) {
                    channelData.clear();
                }
                fitPlotToData = true;
                plotDataCache.dataRevision = numeric_limits<uint64_t>::max();
                hasLoadedFile = false;
                fileError.clear();
                lastFileCheck = chrono::steady_clock::now() - fileCheckInterval;
            }
        }
        
        ImGui::SameLine();
        if (filePath.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), " No file loaded. Please select a valid text log.");
        } else {
            ImGui::Text(" Active File: %s (Points: %llu)", filePath.c_str(), timeData.size());
        }
        ImGui::Spacing();

        // HARDWARE GRAPH PLOT DESIGN WITH ABSOLUTE HISTORICAL TIMELINE
        if (ImPlot::BeginPlot("Live Sensor Timeline Feed", ImVec2(-1, -1))) {
            ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
            ImPlot::SetupAxis(ImAxis_X1, "Timeline (Date/Time)");
            ImPlot::SetupAxis(ImAxis_Y1, "Temperature (°C)");
            
            // Formats display to show Month/Day Hour:Minute (e.g. "02/04 14:46")
            ImPlot::SetupAxisFormat(ImAxis_X1, "%Y-%m-%d %H:%M");

            if (!timeData.empty()) {
                double xMin;
                double xMax;
                if (fitPlotToData) {
                    const double firstTimestamp = timeData.front();
                    const double lastTimestamp = timeData.back();
                    const double padding = max(1.0, (lastTimestamp - firstTimestamp) * 0.01);
                    xMin = firstTimestamp - padding;
                    xMax = lastTimestamp + padding;
                    ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);
                } else {
                    const ImPlotRect limits = ImPlot::GetPlotLimits(ImAxis_X1);
                    xMin = isfinite(limits.X.Min) ? limits.X.Min : timeData.front();
                    xMax = isfinite(limits.X.Max) ? limits.X.Max : timeData.back();
                }
                const int pixelWidth = max(1, static_cast<int>(ImPlot::GetPlotSize().x));

                if (plotDataCache.xMin != xMin || plotDataCache.xMax != xMax ||
                    plotDataCache.pixelWidth != pixelWidth ||
                    plotDataCache.dataRevision != dataRevision) {
                    for (size_t i = 0; i < temperatureChannels.size(); ++i) {
                        BuildPlotSeries(
                            timeData,
                            temperatureData[i],
                            xMin,
                            xMax,
                            pixelWidth,
                            plotDataCache.series[i]);
                    }
                    plotDataCache.xMin = xMin;
                    plotDataCache.xMax = xMax;
                    plotDataCache.pixelWidth = pixelWidth;
                    plotDataCache.dataRevision = dataRevision;
                }

                for (size_t i = 0; i < temperatureChannels.size(); ++i) {
                    ImPlot::PlotLine(
                        temperatureChannels[i].label.c_str(),
                        plotDataCache.series[i].timestamps.data(),
                        plotDataCache.series[i].temperatures.data(),
                        static_cast<int>(plotDataCache.series[i].timestamps.size()));
                }
                fitPlotToData = false;
            }
            ImPlot::EndPlot();
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
