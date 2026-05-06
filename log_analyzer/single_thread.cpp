#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <iomanip>
#include <chrono>
#include <ctime>

struct LogRecord {
    std::string timestamp;
    std::string level;
    std::string tag;
    std::string message;
};

struct ErrorEvent {
    std::string timestamp;
    std::string level;
    std::string message;
};

struct AnalysisResult {
    size_t totalLines = 0; // unsigned_long으로 메모리 할당 가능한 가장 큰 객체 크기 담기 가능
    size_t parsedLines = 0;
    size_t ignoredLines = 0;

    std::unordered_map<std::string, size_t> levelCount;
    std::unordered_map<std::string, size_t> eventCount;
    std::unordered_map<std::string, size_t> errorEventCount;
    std::vector<std::streampos> errorOffsets; 

    std::vector<ErrorEvent> errorEvents;
};

std::string convertLevel(const char& levelChar) {
    switch (levelChar) {
        case 'V': return "VERBOSE";
        case 'D': return "DEBUG";
        case 'I': return "INFO";
        case 'W': return "WARN";
        case 'E': return "ERROR";
        default: return "UNKNOWN";
    }
}

bool isErrorLevel(const std::string& level) {
    return level == "ERROR";
}

std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm* parts = std::localtime(&now_c);

    char buffer[64];
    // YYYY-MM-DD_HH-MM-SS
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d_%H-%M-%S", parts);
    return std::string(buffer);
}

// bool parseLogLine(const std::string& line, LogRecord& outRecord) {
//     std::istringstream iss(line);
//     // 빈칸 기준으로 split과 동일.
//     // 스트림에서 단어를 하나씩 꺼내서 오른쪽에 있는 변수에 저장
//     std::string date;
//     std::string time;
//     std::string uid;
//     std::string gid;
//     char levelChar;
//     std::string tagWithColon;
//     std::string eventName;

//     // if (!(iss >> date >> time >> uid >> gid >> levelChar >> tagWithColon >> eventName)) {
//     //     return false;
//     // }

//     // if (tagWithColon.empty() || tagWithColon.back() != ':') {
//     //     return false;
//     // }

//     // std::string tag = tagWithColon.substr(0, tagWithColon.size() - 1);

//     // std::string message;
//     // std::getline(iss, message);

//     // if (!message.empty() && message.front() == ' ') {
//     //     message.erase(message.begin());
//     // }

//     // outRecord.timestamp = date + " " + time;
//     // outRecord.level = convertLevel(levelChar);
//     // outRecord.tag = tag;
//     // outRecord.eventName = eventName;
//     // outRecord.message = message;

//     if (!(iss >> date >> time >> uid >> gid >> levelChar)) {
//         return false;
//     }

//     std::string remainder;
//     std::getline(iss, remainder); // 앞에꺼 끊고 나머지 가져와서 처리하기

//     size_t colonPos = remainder.find(": ");
//     if(colonPos == std::string::npos) return false; // 못찾으면 skip

//     std::string tag = trim(remainder.substr(0, colonPos));
//     std::string rawMessage = remainder.substr(colonPos + 2);
//     std::string message = "";
//     std::istringstream msgIss(rawMessage);
    
//     std::getline(msgIss, message);
//     if (!message.empty() && message.front() == ' ') {
//         message.erase(0, 1);
//     }

//     outRecord.timestamp = date + " " + time;
//     outRecord.level = convertLevel(levelChar);
//     outRecord.tag = tag;
//     outRecord.message = message;

//     return true;
// }

bool parseLogLine(const std::string& line, LogRecord& outRecord) {
    if (line.empty()) return false;

    size_t pos = 0;
    size_t endPos = 0;

    // date
    endPos = line.find(' ', pos);
    if (endPos == std::string::npos) return false;
    std::string date = line.substr(pos, endPos - pos);
    
    pos = line.find_first_not_of(' ', endPos);
    if (pos == std::string::npos) return false;

    endPos = line.find(' ', pos);
    if (endPos == std::string::npos) return false;
    std::string time = line.substr(pos, endPos - pos);

    // UID, GID - skip
    for (int i = 0; i < 2; ++i) {
        pos = line.find_first_not_of(' ', endPos);
        if (pos == std::string::npos) return false;
        endPos = line.find(' ', pos);
        if (endPos == std::string::npos) return false;
    }

    // Level
    pos = line.find_first_not_of(' ', endPos);
    if (pos == std::string::npos) return false;
    char levelChar = line[pos];
    
    // 레벨 뒤의 공백 스킵
    pos = line.find_first_not_of(' ', pos + 1);
    if (pos == std::string::npos) return false;

    size_t colonPos = line.find(": ", pos);
    if (colonPos == std::string::npos) return false;

    std::string tag = trim(line.substr(pos, colonPos - pos));

    size_t msgStart = colonPos + 2;
    if (msgStart < line.size()) {
        msgStart = line.find_first_not_of(' ', msgStart); 
    }
    
    std::string message = "";
    if (msgStart != std::string::npos && msgStart < line.size()) {
        message = line.substr(msgStart);
    }

    outRecord.timestamp = date + " " + time;
    outRecord.level = convertLevel(levelChar);
    outRecord.tag = tag;
    outRecord.message = message;

    return true;
}

void analyzeRecord(const LogRecord& record, std::streampos offset, AnalysisResult& result) {
    result.parsedLines++;

    result.levelCount[record.level]++;
    result.eventCount[record.tag]++;

    if (isErrorLevel(record.level)) {
        ErrorEvent errorEvent;
        errorEvent.timestamp = record.timestamp;
        errorEvent.level = record.level;
        errorEvent.message = record.message;

        result.errorEventCount[record.tag]++;
        // result.errorEvents.push_back(errorEvent);
        result.errorOffsets.push_back(offset); 
    }
}

AnalysisResult analyzeFile(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filePath);
    }

    AnalysisResult result;
    std::string line;
    std::streampos currentOffset = 0; // 현재 읽을 줄의 바이트 시작 위치

    while (std::getline(file, line)) {
        currentOffset = file.tellg(); // 줄을 읽기 전에 현재 위치를 저장
        if (!std::getline(file, line)) {
            break;
        }

        result.totalLines++;
        LogRecord record;
        if(!parseLogLine(line, record)){
            result.ignoredLines++;
            continue;
        }

        analyzeRecord(record, currentOffset, result);
    }

    return result;
}

std::vector<std::pair<std::string, size_t>> sortByCountDesc(
    const std::unordered_map<std::string, size_t>& map
) {
    std::vector<std::pair<std::string, size_t>> items(map.begin(), map.end());

    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // 내림차순 이벤트 정렬
    });

    return items;
}

void printResult(const AnalysisResult& result) {
    std::cout << "===== Android Log Analysis Result =====\n\n";

    std::cout << "Total lines : " << result.totalLines << "\n";
    std::cout << "Parsed lines: " << result.parsedLines << "\n";
    std::cout << "Ignored lines: " << result.ignoredLines << "\n\n";

    std::cout << "Level Count:\n";
    auto sortedLevels = sortByCountDesc(result.levelCount);
    for (const auto& [level, count] : sortedLevels) {
        std::cout << level << " : " << count << "\n";
    }

    std::cout << "\nEvent Count:\n";
    auto sortedEvents = sortByCountDesc(result.eventCount);
    for (const auto& [eventName, count] : sortedEvents) {
        std::cout << eventName << " : " << count << "\n";
    }

    std::cout << "\nError Events:\n";
    if (result.errorEvents.empty()) {
        std::cout << "No error events found.\n";
        return;
    }

    for (const auto& error : result.errorEvents) {
        std::cout
            << "[" << error.timestamp << "] "
            << error.level << " "
            << error.message << "\n";
    }
}

void saveResultToFile(const std::string& filename, const AnalysisResult& result, const std::string& originalFilePath) {
    std::ofstream outFile(filename);

    if (!outFile.is_open()) {
        std::cerr << "can't open " << filename << std::endl;
        return;
    }

    outFile << "===== Android Log Analysis Result =====\n\n";

    outFile << "Total lines : " << result.totalLines << "\n";
    outFile << "Parsed lines: " << result.parsedLines << "\n";
    outFile << "Ignored lines: " << result.ignoredLines << "\n\n";

    outFile << "Level Count:\n";
    auto sortedLevels = sortByCountDesc(result.levelCount);
    for (const auto& [level, count] : sortedLevels) {
        outFile << level << " : " << count << "\n";
    }

    outFile << "\nError Producers (Tag : Error Count / Total Count):\n\n";
    auto sortedErrorEvents = sortByCountDesc(result.errorEventCount);
    size_t errCount = 0;
    for (const auto& [tag, errors] : sortedErrorEvents) {
        size_t total = result.eventCount.at(tag); // tag가 참조형으로 넘어오기 때문에 at을 써줘야
        double errorRate = (double)errors / total * 100.0;

        outFile << tag << " : " << errors << " / " << total << " (" << std::fixed << std::setprecision(2) << errorRate << "%)\n";
        if (++errCount >= 20) break;
    }

    // outFile << "\nEvent Count:\n";
    
    // auto sortedEvents = sortByCountDesc(result.eventCount);
    // for (const auto& [eventName, count] : sortedEvents) {
    //     outFile << eventName << " : " << count << "\n";
    // }

    outFile << "\nError Events:\n";
    // if (result.errorEvents.empty()) {
    //     outFile << "No error events found.\n";
    // } else {
    //     for (const auto& error : result.errorEvents) {
    //         outFile << "[" << error.timestamp << "] "
    //                 << error.level << " "
    //                 << error.message << "\n";
    //     }
    // }
    if (result.errorOffsets.empty()) {
        outFile << "No error events found.\n";
    } else {
        // 원본 로그 파일을 다시 엽니다.
        std::ifstream originalFile(originalFilePath);
        if (originalFile.is_open()) {
            std::string errorLine;

            for (std::streampos offset : result.errorOffsets) {
                originalFile.seekg(offset); // 해당 바이트 위치로 포인터 이동
                std::getline(originalFile, errorLine); // 그 한 줄만 읽기
                outFile << errorLine << "\n";
            }
        } else {
            outFile << "Failed to open original file to fetch error events.\n";
        }
    }



    outFile.close();
    std::cout << filename << " saved.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: android_log_analyzer <log_file_path>\n";
        return 1;
    }

    try {
        std::string filePath = argv[1];

        AnalysisResult result = analyzeFile(filePath);

        // printResult(result);
        std::string fileName = "./out/analysis_" + getCurrentTimeStr() + ".txt";
        saveResultToFile(fileName, result, filePath);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}