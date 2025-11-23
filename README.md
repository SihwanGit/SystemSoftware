# SIC Macro Processor (C / C++ Implementation)

This project implements a simple macro processor for the SIC assembler, written in both C and C++.</br>
It recognizes macro definitions (MACRO / MEND), replaces parameters, and expands macro calls into full assembly code.</br>

📁 Project Structure
```
.
├── macro_processor.c      # C implementation
├── macro_processor.cpp    # C++ implementation
├── input.asm              # Example assembly file containing a macro
└── README.md              # Documentation
```

🧩 Build and Run</br>
🔹 C version</br>
```
gcc -O2 -o macro_processor macro_processor.c</br>
./macro_processor < input.asm > output.asm</br>
```

🔹 C++ version</br>
```
g++ -std=c++17 -O2 -o macro_processor macro_processor.cpp</br>
./macro_processor < input.asm > output.asm</br>
```
