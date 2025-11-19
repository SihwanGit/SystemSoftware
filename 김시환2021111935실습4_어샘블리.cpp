#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "argc를 불러오지 못함" << endl;
        cerr << "프로젝트 속성 -> 구성속성 -> 디버깅 -> 명령인수에서 열고자 하는 파일 경로 입력" << endl;
        return 1;
    }

    ifstream src(argv[1]);
    ofstream out(argv[2]);

    if (!src.is_open() || !out.is_open()) {
        cerr << "파일 열기 실패" << endl;
        return 1;
    }

    string line;
    int lineNum = 1;
    int loc = 0;
    bool started = false;   // START 처리 여부

    while (getline(src, line)) {
        if (line.empty()) continue;

        string label, opcode, operand;
        stringstream ss(line);

        if (isspace(line[0])) {
            label = "";
            ss >> opcode >> operand;
        }
        else {
            ss >> label >> opcode >> operand;
        }

        // START 만나면 LOC 초기화
        if (opcode == "START" && !started) {
            loc = stoi(operand, nullptr, 16);   //시작주소를 16진수로 해석
            started = true;
        }

        // LOC 출력 문자열 (HEX, 4자리, 0채움)
        stringstream locss;
        locss << uppercase << hex << setw(4) << setfill('0') << loc;

        // 출력
        out << left << setw(4) << lineNum << "\t"      // 라인 번호
            << left << setw(4) << locss.str() << "\t"  // LOC
            << left << setw(10) << label << "\t"       // LABEL
            << left << setw(10) << opcode << "\t"      // OPCODE
            << left << setw(10) << operand             // OPERAND
            << "\n";

        // LOC 계산
        if (opcode == "WORD") {
            loc += 3;
        }
        else if (opcode == "RESW") {
            loc += 3 * stoi(operand, nullptr, 16);
        }
        else if (opcode == "RESB") {
            loc += stoi(operand, nullptr, 16);
        }
        else if (opcode == "BYTE") {
            if (!operand.empty() && operand[0] == 'C') {
                loc += operand.size() - 3;   //문자열 길이만큼
            }
            else if (!operand.empty() && operand[0] == 'X') {
                loc += (operand.size() - 3) / 2;  //2 hex digit = 1 byte
            }
        }
        else if (opcode == "START" || opcode == "END") {
            // 주소 변화 없음
        }
        else {
            loc += 3;  // 일반 명령어 길이 = 3
        }

        lineNum++;
    }

    src.close();
    out.close();

    cout << "intfile 파일 작성이 완료되었습니다." << endl;
    return 0;
}
