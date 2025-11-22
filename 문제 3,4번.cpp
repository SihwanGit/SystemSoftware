#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <ctime>
using namespace std;

#define MEM_SIZE 32758

// 문자열의 앞뒤 공백 제거 함수
string trim(const string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 16진수 문자열을 10진수 정수로 변환
unsigned int hexToDec(const string& hexStr) {
    unsigned int val = 0;
    for (char c : hexStr) {
        val <<= 4;
        if (c >= '0' && c <= '9') val += c - '0';
        else if (c >= 'A' && c <= 'F') val += c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') val += c - 'a' + 10;
    }
    return val;
}

// 10진수 정수를 16진수 문자열로 변환 (대문자, 6자리 고정)
string decToHex(unsigned int val, int width = 6) {
    stringstream ss;
    ss << uppercase << hex << val;
    string s = ss.str();
    if ((int)s.size() > width)
        s = s.substr(s.size() - width); // 초과 시 상위 자리만 유지
    while ((int)s.size() < width)
        s = "0" + s; // 부족 시 앞에 0을 채움
    return s;
}

// 3바이트 오브젝트 코드를 6자리 16진수 문자열로 변환
string toObjString(unsigned int value) {
    stringstream ss;
    ss << uppercase << hex;
    ss.width(6);
    ss.fill('0');
    ss << value;
    return ss.str();
}

// Loader 클래스 정의
class Loader {
private:
    char mem[MEM_SIZE];                 // 가상의 메모리 공간
    string progName;                    // 프로그램 이름
    unsigned int progStart;             // 프로그램 시작 주소
    unsigned int progLen;               // 프로그램 길이
    unsigned int execAddr;              // 실행 시작 주소
    unsigned int randStart;             // 랜덤 로드 시작 주소
    vector<string> relocatedTextRecords;// 재배치된 T 레코드 저장 벡터

public:
    // 생성자: 메모리 초기화
    Loader() {
        memset(mem, 0, sizeof(mem));
        progName = "NONAME";
        progStart = progLen = execAddr = randStart = 0;
    }

    // OBJFILE을 읽고 재배치 수행
    void loadObjectFile(const string& filename) {
        ifstream fin(filename);
        if (!fin.is_open()) {
            cerr << "파일을 열 수 없습니다.\n";
            return;
        }
        string line;

        // H 레코드 읽기 (프로그램 기본 정보)
        if (getline(fin, line)) {
            // UTF-8 BOM 제거
            if (line.size() >= 3 &&
                (unsigned char)line[0] == 0xEF &&
                (unsigned char)line[1] == 0xBB &&
                (unsigned char)line[2] == 0xBF)
                line = line.substr(3);

            // 개행, 탭 등 제거
            line.erase(remove(line.begin(), line.end(), '\r'), line.end());
            line.erase(remove(line.begin(), line.end(), '\n'), line.end());
            line.erase(remove(line.begin(), line.end(), '\t'), line.end());

            // H TEST 001000 000016 형태로 파싱
            string type, name, start, len;
            stringstream ss(line);
            ss >> type >> name >> start >> len;

            if (type == "H") {
                progName = name;
                progStart = hexToDec(start);
                progLen = hexToDec(len);

                cout << "프로그램 이름: " << progName << endl;
                cout << "프로그램 시작주소: " << decToHex(progStart, 6) << endl;
                cout << "프로그램 길이: " << decToHex(progLen, 6) << endl;
            }
            else {
                cerr << "H 레코드가 아닙니다: " << line << endl;
            }
        }

        // 프로그램의 실제 로드 위치를 랜덤하게 선택
        srand((unsigned int)time(NULL));
        randStart = rand() % (MEM_SIZE - progLen);
        cout << "랜덤 시작주소: " << decToHex(randStart, 6) << endl;

        // 이후 레코드들 처리
        while (getline(fin, line)) {
            line = trim(line);
            if (line.empty()) continue;

            stringstream ss(line);
            string type;
            ss >> type;

            // T 레코드 처리 (텍스트 레코드)
            if (type == "T") {
                string startAddrStr, lengthStr, maskStr;
                ss >> startAddrStr >> lengthStr >> maskStr;

                unsigned int recStart = hexToDec(startAddrStr) + randStart; // 재배치 후의 시작주소
                unsigned int maskVal = hexToDec(maskStr);                   // 재배치 비트마스크 값
                unsigned int maskBits = maskStr.size() * 4;                 // 비트 수 (16진수 1자리=4비트)

                string objCode;
                vector<string> relocatedObjs;
                unsigned int bitIndex = 0;

                // 각 오브젝트 코드 읽기
                while (ss >> objCode) {
                    unsigned int value = hexToDec(objCode);
                    bool reloc = false;

                    // 재배치 비트마스크의 각 비트를 확인
                    // 비트가 1이면 해당 오브젝트 코드의 주소를 수정해야 함
                    if (bitIndex < maskBits)
                        reloc = ((maskVal >> (maskBits - 1 - bitIndex)) & 1);

                    // 재배치 수행
                    if (reloc) {
                        // 24비트 오브젝트 코드 구조: [8비트 opcode][4비트 nixb][12비트 address]
                        unsigned int opcode = value & 0xFF0000; // 명령어 코드
                        unsigned int nixb = value & 0x00F000;   // 어드레싱 모드 비트
                        unsigned int addr = value & 0x000FFF;   // 주소 필드

                        // 주소에 로드 시작주소를 더해 재배치
                        addr = (addr + randStart) & 0xFFF;

                        // 재조합
                        value = opcode | nixb | addr;
                    }

                    // 변환된 6자리 16진수 문자열로 저장
                    relocatedObjs.push_back(toObjString(value));
                    bitIndex++;
                }

                // 재배치된 T 레코드 구성
                string newRec = "T " + decToHex(recStart - randStart, 6)
                    + " " + lengthStr + " " + maskStr;
                for (auto& obj : relocatedObjs)
                    newRec += " " + obj;

                relocatedTextRecords.push_back(newRec);
            }

            // E 레코드 처리 (실행 주소)
            else if (type == "E") {
                string addr;
                ss >> addr;
                execAddr = hexToDec(addr);
            }
        }

        fin.close();
    }

    // 재배치된 Object Program 출력
    void printResult() const {
        cout << "\n===== 재배치된 Object Program =====\n";
        cout << "H " << progName << " "
            << decToHex(progStart + randStart, 6) << " "
            << decToHex(progLen, 6) << endl;

        for (const string& rec : relocatedTextRecords)
            cout << rec << endl;

        cout << "E " << decToHex(execAddr + randStart, 6) << endl;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    Loader loader;                      // 로더 객체 생성
    loader.loadObjectFile("OBJFILE.txt"); // OBJFILE 읽기 및 재배치 수행
    loader.printResult();                // 결과 출력

    return 0;
}
