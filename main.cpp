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
#include <ctime>
#include <windows.h>
#include <commdlg.h>

std::vector<double> timeData;
std::vector<double> tempData;

struct TemperatureEntry {
    double timestamp; // Absolute Unix Epoch seconds
    double temperature;
};

// Converts "YYYY-MM-DD HH:MM:SS" directly into absolute Unix Epoch Seconds
double DateStringToEpoch(const std::string& dateStr) {
    std::tm t = {};
    std::stringstream ss(dateStr);
    char dash, colon;
    
    // Parse expected format: 2026-02-04 14:46:20
    if (ss >> t.tm_year >> dash >> t.tm_mon >> dash >> t.tm_mday 
           >> t.tm_hour >> colon >> t.tm_min >> colon >> t.tm_sec) 
    {
        t.tm_year -= 1900; // std::tm expects years since 1900
        t.tm_mon -= 1;     // std::tm months are 0-indexed (0 = January)
        t.tm_isdst = -1;   // Let the system determine Daylight Saving Time Automatically
        
        std::time_t epochTime = std::mktime(&t);
        if (epochTime != -1) {
            return static_cast<double>(epochTime);
        }
    }
    return -1.0; // Return error sentinel if parsing fails
}

TemperatureEntry ParseLine(const std::string& line) {
    TemperatureEntry entry{-1.0, 0.0};
    if (line.empty()) return entry;

    // 1. Extract the full timestamp string block (first 19 characters)
    // Example format: "2026-02-04 14:46:20"
    if (line.length() >= 19) {
        std::string fullDateStr = line.substr(0, 19); 
        entry.timestamp = DateStringToEpoch(fullDateStr);
    } else {
        return entry; // Line too short
    }

    // 2. Locate the precise text token key wrapper: "\"TempR2\":"
    std::string key = "\"TempR2\":";
    size_t keyPos = line.find(key);
    
    if (keyPos != std::string::npos) {
        size_t valueStart = keyPos + key.length();
        size_t commaPos = line.find(',', valueStart);
        
        if (commaPos != std::string::npos) {
            std::string tempStr = line.substr(valueStart, commaPos - valueStart);
            try {
                entry.temperature = std::stod(tempStr);
            } catch (...) {
                entry.timestamp = -1.0; // Flag as error
            }
        }
    } else {
        entry.timestamp = -1.0; // Key wasn't found
    }

    return entry;
}

// Function to trigger the built-in Windows Open File Explorer window
std::string OpenFileDialog() {
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
        return std::string(ofn.lpstrFile);
    }
    return ""; // Return empty string if the user cancelled the action
}

// Helper function to get the exact directory where the .exe file sits
std::string GetExecutableDirectory() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

int main() {
    // Force the file path to exist directly next to the launching executable binary (.exe)
    std::string filePath = GetExecutableDirectory() + "\\temperature_log.txt";
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::ofstream create_file(filePath);
        // Seed base entry matched to real parse expectations
        create_file << "2026-02-04 12:00:00,\"TempR2\":20.0,\n"; 
        create_file.close();
        file.open(filePath);
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
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::string line;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // READ FROM THE AUTOMATIC PATH LOCATION & STREAM DATA CONTINUOUSLY
        if (!filePath.empty() && file.is_open()) {
            while (std::getline(file, line)) {
                TemperatureEntry data = ParseLine(line);
                if (data.timestamp >= 0.0) { // Verify timeline conversions succeeded
                    timeData.push_back(data.timestamp);
                    tempData.push_back(data.temperature);
                }
            }
            file.clear(); 
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
        ImGui::Separator();

        // 2. FILE SELECTION INTERFACE AREA
        if (ImGui::Button("Browse Log File...", ImVec2(200, 30))) {
            std::string selectedFile = OpenFileDialog();
            if (!selectedFile.empty()) {
                filePath = selectedFile;
                
                // Clear old history data vectors instantly to plot the fresh file cleanly
                timeData.clear();
                tempData.clear();
                
                // Open the new file stream target cleanly
                if (file.is_open()) file.close();
                file.open(filePath);
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
            ImPlot::SetupAxisFormat(ImAxis_X1, "%m/%d %H:%M");

            if (!timeData.empty()) {
                ImPlot::PlotLine("Room 2 Temp", timeData.data(), tempData.data(), (int)timeData.size());
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

    if (file.is_open()) file.close();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
