#include <iostream>
#include <vector>
#include <iomanip>
#include <bitset>
#include <cstdlib>
#include <fstream> // 프로그램 파일 입력을 위해 추가

using namespace std;

/* ================= Register Base ================= */

class Register {
protected:
    uint16_t value;
public:
    Register() : value(0) {}
    virtual ~Register() {}
    void set(uint16_t v) { value = v; }
    uint16_t get() const { return value; }
};

/* ================= Specific Registers ================= */

class PC : public Register {
    // Program Counter 전용 레지스터
};

class IR : public Register {
    // Instruction Register
};

class AC : public Register {
    // Accumulator
};

/* ================= ALU ================= */

class ALU {
public:
    uint16_t add(uint16_t a, uint16_t b) { return a + b; }
    uint16_t mul(uint16_t a, uint16_t b) { return a * b; }
    uint16_t div(uint16_t a, uint16_t b) { return b == 0 ? 0 : a / b; }
    uint16_t mod(uint16_t a, uint16_t b) { return b == 0 ? 0 : a % b; }
};

/* ================= Control Unit ================= */

class CU {
public:
    uint16_t opcode(uint16_t instr) {
        return (instr >> 12) & 0xF;
    }
    uint16_t operand(uint16_t instr) {
        return instr & 0x0FFF;
    }
};

/* ================= Memory ================= */

class Memory {
private:
    vector<uint8_t> mem;       // 실제 메모리 공간 (4096 byte)
    unsigned int mpt;          // 메모리 포인터 (0~4095)

public:
    Memory() : mem(4096, 0), mpt(0) {}   // 메모리 초기화 및 포인터 0 설정


    //========== 메모리 포인터 설정 ===========

    // 메모리 포인터를 시작주소(0)로 초기화
    void resetPointer() {
        mpt = 0;
    }

    // 메모리 포인터를 임의의 입력한 주소로 설정
    void setPointer(unsigned int addr) {
        if (valid(addr))
            mpt = addr;
    }

    // 메모리 주소 유효성 검사
    bool valid(unsigned int addr) const {
        return addr < 4096;
    }


    //========== 읽기(read) 연산 ==========

    // 특정 주소에서 16bit 정수 읽기
    uint16_t read(unsigned int addr) const {
        if (addr + 1 >= 4096) return 0;
        return (mem[addr] << 8) | mem[addr + 1];
    }

    // 현재 포인터 위치에서 16bit 정수 읽기
    uint16_t read() {
        uint16_t value = read(mpt);
        mpt += 2;
        return value;
    }

    // 메모리 첫 주소(0)부터 16bit 정수 읽기
    uint16_t readFromStart() {
        mpt = 0;
        return read();
    }

    //========== 쓰기(write) 연산 ==========

    // 특정 주소에 16bit 정수 쓰기
    void write(unsigned int addr, uint16_t data) {
        if (addr + 1 >= 4096) return;
        mem[addr] = (data >> 8) & 0xFF;
        mem[addr + 1] = data & 0xFF;
    }

    // 현재 포인터 위치에 16bit 정수 쓰기
    void write(uint16_t data) {
        write(mpt, data);
        mpt += 2;
    }

    // 메모리 첫 주소(0)부터 16bit 정수 쓰기
    void writeFromStart(uint16_t data) {
        mpt = 0;
        write(data);
    }

    //========== 초기화 연산 ==========

    // 특정 주소의 16bit 데이터를 0으로 초기화
    void clearAt(unsigned int addr) {
        if (addr + 1 >= 4096) return;
        mem[addr] = 0;
        mem[addr + 1] = 0;
    }

    // 메모리 전체를 0으로 초기화
    void clearAll() {
        fill(mem.begin(), mem.end(), 0);
        mpt = 0;
    }

    // 메모리 전체를 임의의 값으로 초기화
    void randomInit() {
        for (auto& b : mem)
            b = rand() % 256;
        mpt = 0;
    }

    //=========== 메모리 내용보기 ==========

    // 메모리 내용 출력
    void dump(unsigned int start, unsigned int end, int base) const {
        for (unsigned int i = start; i <= end && i + 1 < 4096; i += 2) {
            uint16_t value = read(i);
            cout << "[" << setw(4) << i << "] ";
            if (base == 2)
                cout << bitset<16>(value);
            else
                cout << hex << setw(4) << setfill('0') << value << dec;
            cout << endl;
        }
    }

    // dump 함수만으로 모든 전체/임의 범위에 대한 출력 기능과 2/16진법 출력도 만족한다.
    // 이 아래의 함수들은 그냥 dump()를 편하게 사용하기 위한 인터페이스 함수들이다.
    // 기능적으론 dump로도 모두 가능하므로 그냥 삭제해도 된다.

    // 메모리 전체 내용 보기 (기본 16진수)
    void dumpAll() const {
        dump(0, 4095, 16);
    }

    // 임의 범위 메모리 내용 보기 (기본 16진수)
    void dumpRange(unsigned int start, unsigned int end) const {
        dump(start, end, 16);
    }

    // 메모리 전체 내용 보기 (2진수 출력)
    void dumpAllBinary() const {
        dump(0, 4095, 2);
    }

    // 임의 범위 메모리 내용 보기 (2진수 출력)
    void dumpRangeBinary(unsigned int start, unsigned int end) const {
        dump(start, end, 2);
    }

};


/* ================= CPU16 ================= */

class CPU16 {
private:
    ALU alu;
    CU cu;

    PC pc;
    IR ir;
    AC ac;

    unsigned int programEnd; // 로드된 프로그램의 끝 주소

public:
    Memory memory;

    void loadProgram(const vector<uint16_t>& program) { // 메모리에 프로그램 적재
        memory.resetPointer();
        for (auto instr : program)
            memory.write(instr);

        programEnd = program.size() * 2; // 프로그램 마지막 다음 주소
    }

    void execute() {
        pc.set(0);

        while (pc.get() < programEnd) {

            // PC가 유효한 메모리 범위를 벗어나는 경우
            if (!memory.valid(pc.get()) || !memory.valid(pc.get() + 1)) {
                cout << "PC out of memory range\n";
                return;
            }

            ir.set(memory.read(pc.get()));
            pc.set(pc.get() + 2);

            uint16_t instr = ir.get();
            uint16_t op = cu.opcode(instr);
            uint16_t x = cu.operand(instr);

            switch (op) {

            case 0x0: // LDA x : 메모리 [x:x+1] -> AC
                if (!memory.valid(x) || !memory.valid(x + 1)) {
                    cout << "LDA address out of range\n";
                    return;
                }
                ac.set(memory.read(x));
                break;

            case 0x1: // STA x : AC -> 메모리 [x:x+1]
                if (!memory.valid(x) || !memory.valid(x + 1)) {
                    cout << "STA address out of range\n";
                    return;
                }
                memory.write(x, ac.get());
                break;

            case 0x2: // ADD x : 즉시값 덧셈
                ac.set(alu.add(ac.get(), x));
                break;

            case 0x3: // MUL x : 즉시값 곱셈
                ac.set(alu.mul(ac.get(), x));
                break;

            case 0x4: // DIV x : 즉시값 나눗셈
                ac.set(alu.div(ac.get(), x));
                break;

            case 0x5: // MOD x : 즉시값 나머지
                ac.set(alu.mod(ac.get(), x));
                break;

            case 0xF: // SEA x : 즉시값 로드
                ac.set(x);
                break;

            default:
                cout << "Unknown opcode\n";
                return;
            }
        }
    }
};


/* ================= Main ================= */

int main() {
    CPU16 cpu;
    vector<uint16_t> program; // 프로그램을 파일에서 읽어 저장

    ifstream fin("program.txt"); // 프로그램을 외부 파일에서 입력
    if (fin.is_open()) {
        uint16_t instr;
        while (fin >> hex >> instr) {
            program.push_back(instr); // 한 줄당 하나의 16bit 기계어 명령어
        }
        fin.close();
    }

    if (program.empty()) {
        cpu.memory.clearAll(); // 프로그램이 입력되지 않으면 메모리를 임의로 초기화
        //clearAll() 말고도 randomInit()으로 하면 랜덤값이 들어감.
    }
    else {
        cpu.loadProgram(program); // 프로그램이 존재하면 메모리에 적재
        cpu.execute(); // 적재된 프로그램 실행
    }

    cout << "\n[실행 후 메모리]\n";
    cpu.memory.dumpRange(0, 20); //dump(0, 16, 16)으로 해도 됨.

    cout << "\n[2진수 출력]\n";
    cpu.memory.dumpRangeBinary(0, 20); //dump(0, 16, 2)으로 해도 됨.

    return 0;
}
