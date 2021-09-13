#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h> 
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#define BUFSIZE 1024                //버프 사이즈                                              //파일 전송시 최대 크기 지정
#define HOME /index.html                                                     

void* request_handler();                                                   //HTTP request를 해석해서 응답에 필요한 소스를 추출
char* send_data(char* body, char* buffer, int buflen);                               //req에 맞는 파일 타입과 파일을 accept로 생성된 소켓에 담아 response
char* content_type(char* buffer, int buflen);                                                  //요청하는 파일 타입을 리턴

struct stat s;                              //아래에서 파일 크기를 구하기 위해 사용
struct {                                    //구조체
    char* ext;                              //char 형식 확장자
    char* filetype;                         //char 형식 파일 타입
}extensions[] = {
    {"gif", "image/gif" },  //gif 
	{"jpg", "image/jpg"},    //jpg
	{"jpeg","image/jpeg"},   //jpeg
	{"png", "image/png" },  //png
	{"htm", "text/html" },  //htm
	{"html", "text/html" },  //html
    {"pdf", "application/pdf"},  //pdf
    {"mp3", "audio/mpeg"},  //mp3
    {0,0}
};

int main(int argc, char *argv[]) {
    int serv_sock, clnt_sock;                                                       //tcp handshake를 위한 소켓과 accept로 생성될 소켓 변수 선언
    struct sockaddr_in serv_adr, clnt_adr;                                          //소켓 정보를 담을 구조체 선언
    int clnt_adr_size;                                                              //clnt_adr을 담을 버퍼를 위한 사이즈 변수 선언
    
    if(argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);                                     //input 인자 포맷 명시
        exit(1);
    }

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);                                    //handshake를 위한 소켓 생성 및 저장
    memset(&serv_adr, 0, sizeof(serv_adr));                                         //변수 초기화
    serv_adr.sin_family = AF_INET;                                                  //IPv4 설정
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);                                   //임의의 IP주소 부여
    serv_adr.sin_port = htons(atoi(argv[1]));                                       //인자로 입력받은 포트번호 부여
    if(bind(serv_sock, (struct sockaddr*)&serv_adr, sizeof(serv_adr)) == -1) {      //serv_sock을 serv_adr과 연동(listen 사전 준비)
        error_handling("bind() error");
    }
    if(listen(serv_sock, 20) == -1) {                                               //HTTP req를 받을 준비 완료
        error_handling("listen() error");
    }

    while(1) {
        
        clnt_adr_size = sizeof(clnt_adr);                                           //클라이언트와 통신할 소켓 크기 지정
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_size); //클라이언트 통신 소켓을 accept로 받아 생성

        request_handler(clnt_sock);                                                 //클라이언트에게서 받은 정보를 처리
    }

    return 0;
}

void* request_handler(int arg1) { 
    char file_name[50];                                                        //파일명 담을 변수
    int j, file_fd, buflen, len, i, ret;                                    //각종 int형 변수
    int clnt_sock = arg1;                                                   //클라이언트 req 정보를 인자를 통해 상속받음
    int clnt_read;                                                           //read를 위한 파일 선언
    char buffer[BUFSIZE + 1];                                               //req 및 res를 담을 버퍼
        
    clnt_read = read(clnt_sock, buffer, BUFSIZE);                           //read로 req의 요청파일 정보를 받는다
    char temp_buf[512];                                                     //req 메세지 출력을 위한 버퍼
    for(int i = 0; i <= 512; i++) {
        temp_buf[i] = buffer[i];                                            
    }
    temp_buf[512] = '\0';
    printf("this is req_message : \n\n%s\n", temp_buf);                      //HTTP req 메세지 출력
    printf("\n");
    
    if(clnt_read == 0 || clnt_read == -1) {                                 //read 실패의 경우
        perror("read error");
        exit(1);
    }
    if(clnt_read > 0 && clnt_read < BUFSIZE) {                              //read 성공의 경우
        buffer[clnt_read] = 0;                                              //끝에 NULL 문자를 넣는 것과 같은 개념
    }
    else {                                                                  
        buffer[0] = 0;
    }

    for(i = 4; i <BUFSIZE; i++) {                                           //GET /images/05_08-over.gif 이런식으로 만들어줌 
        if(buffer[i] == ' ') {
            buffer[i] = 0;
            break;
        }
    }
    
    if(!strncmp(&buffer[0], "GET /\0", 6)) {                                //GET /\0일때  
        strcpy(buffer, "GET /index.html");
    }
    buflen = strlen(buffer);                                                //버퍼 길이 계산
    
    strcpy(file_name, &buffer[5]);                                           //file_name 변수에 파일명 저장
    file_fd = open(file_name, O_RDONLY);                                    //읽기전용으로 파일 오픈
    
    fstat(file_fd, &s);                                                     //요청하는 파일 정보를 받아옵니다
    if(file_fd == -1) {                                                     //파일이 아닌경우
        /////////////////////응답 메세지 작성
        char body[] = {"<HTML><BODY><H1>NOT FOUND</H1></BODY></HTML>\r\n"};
        sprintf(buffer, "%s", send_data(&body, &buffer, buflen));
        //////////////////////////////////
        write(clnt_sock, buffer, strlen(buffer));                           //작성한 버퍼를 클라이언트 소켓정보에 담아 내려보냄
        close(clnt_sock);
    }
    else if(S_ISDIR(s.st_mode)) {                                              //파일인 경우
        /////////////////////응답 메세지 작성
        char body[] = {"<HTML><BODY><H1>NOT FOUND</H1></BODY></HTML>\r\n"};
        char* buffer2;
        sprintf(buffer, "%s", send_data(&body, &buffer, buflen));
        //////////////////////////////////
        write(clnt_sock, buffer, strlen(buffer));                           //작성한 버퍼를 클라이언트 소켓정보에 담아 내려보냄
        close(clnt_sock);
    }
    
    /////////////////////응답 메세지 작성
    sprintf(buffer, "%s", send_data(NULL, &buffer, buflen));
    //////////////////////////////////
    write(clnt_sock, buffer, strlen(buffer));                               //작성한 버퍼를 클라이언트 소켓정보에 담아 내려보냄
    while((clnt_read = read(file_fd, buffer, BUFSIZE)) > 0) {
        write(clnt_sock, buffer, clnt_read);                                //요청한 파일 크기에 상관없이 전송할 수 있도록 반복문 사용, 요청 파일 전송이 완료되면 탈출한다
    }
    
    close(clnt_sock);
}

char* send_data(char* body, char* buffer, int buflen) {                               //handler에서 작성한 clnt_write 파일 포인터, content-type, 파일명으로 response 메세지 구성
    char protocol[] = "HTTP/1.1 200 OK\r\n";                                        //res header 구성 벡터
    char server[] = "Server:JaeYub's 1st server \r\n";                              //res header 구성 벡터
    char cnt_len[] = "Content-length:2048\r\n";                                        //res header 구성 벡터
    char cnt_type[BUFSIZE];                                                         //파일 타입을 받을 변수 선언
    
    if(!body) {                                                                          //함수 호출시 넘겨받은 body 인자가 NULL인경우
        sprintf(cnt_type, "Content-Type: %s\r\n\r\n", content_type(buffer, buflen));        //content-type 함수 호출로 파일 타입 버퍼에 입력
        sprintf(buffer, "%s%s", protocol, cnt_type);                                    //HTTP response를 작성합니다
        return buffer;
    }
    else {
        sprintf(cnt_type, "Content-type: text/html\r\n\r\n");                              //cnt_type 벡터에 인자로 넘겨받은 ct 값 입력
        sprintf(buffer, "%s%s%s%s%s", protocol, server, cnt_len, cnt_type, body);           //HTTP response를 작성합니다
        return buffer;
    }
    
}

char* content_type(char* buffer, int buflen) {                                              //파일 타입을 리턴합니다
    int len;
    for(int i = 0; extensions[i].ext != 0; i++) {
        len = strlen(extensions[i].ext);
        if(!strncmp(&buffer[buflen-len], extensions[i].ext, len)) {                          //인자로 넘겨받은 버퍼에서 맞는 확장자를 찾아 리턴합니다.
            return extensions[i].filetype;
            break;
        }
    }     
}

void error_handling(char* message) {                                                                //상황에 따른 에러 메시지 출력
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
