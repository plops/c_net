
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
using namespace std;

class NotInAr{};
class Ar{
public:
  int w,h,n;
  unsigned char*data;
  Ar(int W,int H,void*buf,unsigned int n):w(W),h(H),n(n){
    data=(unsigned char*)buf;
  }
  Ar(const Ar&a):w(a.w),h(a.h),n(a.n),data(a.data){}
  unsigned char&operator()(int c,int i,int j){
    if(i>=w || j>=h)
      throw NotInAr();
    int rows=w*2*j;
    switch(c){
    case 0: // y
      return data[2*i+rows];
    case 1: // u
      return data[4*(i/2)+1+rows];
    case 2: // v
      return data[4*(i/2)+3+rows];
    }
    throw NotInAr();
  }
  void output(char*filename){
    FILE*f=fopen(filename,"w");
    fprintf(f,"P5\n%d %d\n255\n",w,h);
    unsigned char buf[w*h];
    for(int j=0;j<h;j++)
      for(int i=0;i<w;i++){
	buf[i+w*j]=(*this)(0,i,j);
      }
    fwrite(buf,w,h,f);
    fclose(f);
  }
};

#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <vector>
#include <sys/mman.h>

class CamOpenError{};
class V4L {
public:
  int fd;
  struct v4l2_format format;
  vector<Ar* > imgs;
  struct v4l2_requestbuffers reqbuf;

  V4L(int w,int h){
    fd=open("/dev/video0",O_RDWR);
    memset(&format,0,sizeof(format));
    format.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width=w;
    format.fmt.pix.height=h;
    format.fmt.pix.pixelformat=V4L2_PIX_FMT_YUYV;
    //sleep(3);
    if(-1==ioctl(fd,VIDIOC_S_FMT,&format))
      throw CamOpenError();
    
    memset(&reqbuf,0,sizeof(reqbuf));
    reqbuf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory=V4L2_MEMORY_MMAP;
    reqbuf.count=4;
    if(-1==ioctl(fd,VIDIOC_REQBUFS,&reqbuf))
      throw CamOpenError();
    
    for(unsigned int i=0;i<reqbuf.count;i++){
      struct v4l2_buffer buf;
      memset(&buf,0,sizeof(buf));
      
      buf.type=reqbuf.type;
      buf.memory=V4L2_MEMORY_MMAP;
      buf.index=i;
      if(-1==ioctl(fd,VIDIOC_QUERYBUF,&buf))
	throw CamOpenError();

      void*start=mmap(NULL,buf.length,
		      PROT_READ|PROT_WRITE,
		      MAP_SHARED,fd,buf.m.offset);
      if(start==MAP_FAILED)
	throw CamOpenError();
      imgs.push_back(new Ar(format.fmt.pix.width,
			    format.fmt.pix.height,
			    start,buf.length));
      if(-1==ioctl(fd,VIDIOC_QBUF,&buf))
	throw CamOpenError();

      int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
      ioctl(fd,VIDIOC_STREAMON,&type);
      //     if(-1==ioctl(fd,VIDIOC_STREAMON,&type))
      //	throw CamOpenError();
    }
    
  }
  ~V4L(){
    int type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1==ioctl(fd,VIDIOC_STREAMOFF,&type))
      throw CamOpenError();
    for(unsigned int i=0;i<imgs.size();i++)
      munmap((void*)imgs[i]->data,imgs[i]->n);
    close(fd);
  }

  struct v4l2_buffer current_buf;

  Ar&get_frame(){
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd,&fds);
    struct timeval tv;
    tv.tv_sec=10;
    tv.tv_usec=0;
    if(0>=select(fd+1,&fds,NULL,NULL,&tv))
      throw CamOpenError();
    
    memset(&current_buf,0,sizeof(current_buf));
    current_buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
    current_buf.memory=V4L2_MEMORY_MMAP;
    if(-1==ioctl(fd,VIDIOC_DQBUF,&current_buf))
      throw CamOpenError();
      
    return *imgs[current_buf.index];
  }
  void dispose_frame(){
    if(-1==ioctl(fd,VIDIOC_QBUF,&current_buf))
      throw CamOpenError();
  }
};

/** So that I don't have to remember to call dispose_frame
 */
class Frame:public Ar{
  V4L*v;
public:
  Frame(V4L&vv):Ar(vv.get_frame()),v(&vv){}
  ~Frame(){
    v->dispose_frame();
  }
};

class ErBind{};
class ErListen{};
class ErAccept{};
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
      throw(ErBind());
  
    if(listen(fd,10)<0)
      throw(ErListen());
  }
  void accept(){
    socklen_t len=sizeof(addr);
    connfd=::accept(fd,(struct sockaddr*)&addr,&len);
    if(connfd<0)
      throw(ErAccept());

    cout << "connection from "
	 << inet_ntoa(addr.sin_addr) 
	 << " len "
	 << len
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

char s[512*512];
Net n;

void close()
{
  cout << "closing socket" << endl;
  n.close();
}

int
main()
{
  V4L v(640,480);
  cout << "waiting for accept" << endl;
  n.accept();
  atexit(close);
  for(;;){
    {
      Frame f(v);
      //  f.output("out.pgm");
      for(int j=0;j<480;j++)
	for(int i=0;i<512;i++)
	  s[i+512*j]=f(0,i,j);
    }
    cout << "." << endl;
    n.send((unsigned char*)s,sizeof(s));
  }
  return 0;
}
