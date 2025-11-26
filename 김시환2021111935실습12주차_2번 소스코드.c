#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>

int main() {
    int pipe1[2];   // 부모 → 자식
    int pipe2[2];   // 자식 → 부모
    pid_t pid;

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) {
        perror("pipe failed");
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) { // 자식 프로세스

        close(pipe1[1]);  // 부모 → 자식 write 닫기
        close(pipe2[0]);  // 자식 → 부모 read 닫기

        int dan;
        read(pipe1[0], &dan, sizeof(dan));  // 부모가 보낸 단 읽기
        close(pipe1[0]);

        // 구구단 문자열 만들기
        char result[1024];
        memset(result, 0, sizeof(result));

        char line[50];
        for (int i = 1; i <= 9; i++) {
            sprintf(line, "%d * %d = %d\n", dan, i, dan * i);
            strcat(result, line);
        }

        // 부모에게 결과 전달
        write(pipe2[1], result, strlen(result) + 1);
        close(pipe2[1]);

        exit(0);
    }
    else { // 부모 프로세스

        close(pipe1[0]);  // 부모 → 자식 read 닫기
        close(pipe2[1]);  // 자식 → 부모 write 닫기

        int dan;
        printf("구구단 단수를 입력하세요 : ");
        scanf("%d", &dan);

        // 자식에게 단 전달
        write(pipe1[1], &dan, sizeof(dan));
        close(pipe1[1]);

        // 자식이 보낸 결과 읽기
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
        read(pipe2[0], buffer, sizeof(buffer));
        close(pipe2[0]);

        printf("\n===== 구구단 결과 =====\n");
        printf("%s", buffer);

        wait(NULL);  // 자식 종료 대기
    }

    return 0;
}
