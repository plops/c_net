#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
using namespace std;

class Net{
public:
  int fd,connfd; 
  struct sockaddr_in addr;
  Net(int port=1234){
    fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=htonl(INADDR_ANY);
    if(bind(fd,(struct sockaddr*)&addr,sizeof(addr))<0)
      throw("error bind");
  
    if(listen(fd,1)<0)
      throw("error during listening");
  }
  void accept(){
    socklen_t len;
    connfd=::accept(fd,(struct sockaddr*)&addr,&len);
    if(connfd<0){
      cerr<<"accept"<<endl;
      throw("error with accept");
    }
    cout << "connection from "
	 << inet_ntoa(addr.sin_addr) 
	 << endl;
  }
  void close(){
    ::close(fd);
  }
  int send(unsigned char*data,int length){
    int
      bytes_sent=0,
      bytes_left=length;
    while(bytes_left>0){
      int bytes=::send(connfd,data+bytes_sent,bytes_left,0);
      bytes_left-=bytes;
      bytes_sent+=bytes;
    }
    return bytes_sent;
  }
};


int
main()
{
  Net n;
  char s[1024*1024];
  for(int i=0;i<sizeof(s);i++)
    s[i]=i;
  for(;;){
    n.accept();
    n.send((unsigned char*)s,sizeof(s));
    n.close();
  }
  return 0;
}
