// SPDX-License-Identifier: GPL-2.0-only
#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <mini_binder.h>

static int write_cmd(int fd, struct mini_binder_command *cmd) {
#ifdef MINI_BINDER_WRITE_READ
    struct mini_binder_write_read b = {.write_size=sizeof(*cmd), .write_buffer=(uintptr_t)cmd};
    return ioctl(fd, MINI_BINDER_WRITE_READ, &b);
#else
    return ioctl(fd, MINI_BINDER_WRITE, cmd);
#endif
}
static int read_cmd(int fd, struct mini_binder_command *cmd) {
#ifdef MINI_BINDER_WRITE_READ
    struct mini_binder_write_read b = {.read_size=sizeof(*cmd), .read_buffer=(uintptr_t)cmd};
    return ioctl(fd, MINI_BINDER_WRITE_READ, &b);
#else
    return ioctl(fd, MINI_BINDER_READ, cmd);
#endif
}
static void send_action(int fd, unsigned int code) {
    struct mini_binder_command c = {.cmd=code}; assert(write_cmd(fd,&c)==0);
}
static unsigned char *map_fd(int fd) {
    void *p=mmap(NULL,(size_t)sysconf(_SC_PAGESIZE),PROT_READ,MAP_SHARED,fd,0);
    assert(p!=MAP_FAILED); return p;
}
static void put32(unsigned char *p, uint32_t n) {
    for(int i=0;i<4;i++) p[i]=(unsigned char)(n>>(i*8));
}
static uint32_t get32(const unsigned char *p) {
    return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;
}
static void byte_out(int fd) { assert(write(fd,"G",1)==1); }
static void byte_in(int fd) { char b; assert(read(fd,&b,1)==1); }
struct observation { int tid; uint32_t cookie, value; };
static void report(int fd, uint32_t cookie, uint32_t value) {
    struct observation o={(int)syscall(SYS_gettid),cookie,value};
    assert(write(fd,&o,sizeof(o))==sizeof(o));
}
static struct observation observe(int fd) {
    struct observation o; assert(read(fd,&o,sizeof(o))==sizeof(o)); return o;
}
static int gates[2][2], releases[2][2], events[2][2], shared_fd, demo;
static void await_sleeping_ioctl(pid_t pid,int tid) {
    char syscall_path[128], status_path[128], line[512];
    struct timespec tick={.tv_nsec=1000000};
    snprintf(syscall_path,sizeof(syscall_path),"/proc/%d/task/%d/syscall",pid,tid);
    snprintf(status_path,sizeof(status_path),"/proc/%d/task/%d/status",pid,tid);
    for(int i=0;i<5000;i++) {
        long number=-1; int sleeping=0;
        FILE *f=fopen(syscall_path,"r");
        if(f) { if(fgets(line,sizeof(line),f)) sscanf(line,"%ld",&number); fclose(f); }
        f=fopen(status_path,"r");
        if(f) { while(fgets(line,sizeof(line),f)) if(!strncmp(line,"State:",6)) { char state=0; if(sscanf(line,"State: %c",&state)==1) sleeping=state=='S'; } fclose(f); }
        if(number==SYS_ioctl && sleeping) return;
        nanosleep(&tick,NULL);
    }
    assert(!"second worker did not sleep in READ");
}
static void *worker(void *arg) {
    int n=(int)(intptr_t)arg;
    send_action(shared_fd,MINI_BC_ENTER_LOOPER);
    for(int round=0;round<(demo?1:6);round++) {
        struct mini_binder_command request={0}, bad={.cmd=MINI_BC_REPLY};
        byte_in(gates[n][0]); report(events[n][1],0,0);
        assert(read_cmd(shared_fd,&request)==0 && request.cmd==MINI_BR_TRANSACTION);
        assert(request.transaction.flags==MINI_TF_ONE_WAY);
        const unsigned char *data=(const void *)(uintptr_t)request.transaction.data_address;
        if (request.transaction.object_count) {
            assert(get32(data)==7 && request.transaction.object_offsets[0]==4);
            assert(get32(data+4)==MINI_BINDER_OBJECT_FD);
            int transferred=(int)get32(data+8); char byte;
            assert(read(transferred,&byte,1)==1 && byte=='X'); close(transferred);
        }
        report(events[n][1],(uint32_t)request.transaction.object_cookie,get32(data));
        if(!demo) assert(write_cmd(shared_fd,&bad)==-1 && errno==EINVAL);
        byte_in(releases[n][0]);
        if(!demo && round==4 && n==0) {
            assert(ioctl(shared_fd,MINI_BINDER_THREAD_EXIT)==0);
            send_action(shared_fd,MINI_BC_ENTER_LOOPER);
        } else send_action(shared_fd,MINI_BC_FREE_BUFFER);
        bad.cmd=MINI_BC_FREE_BUFFER;
        if(!demo) assert(write_cmd(shared_fd,&bad)==-1 && errno==ENOENT);
        report(events[n][1],0,1);
    }
    assert(ioctl(shared_fd,MINI_BINDER_THREAD_EXIT)==0); return NULL;
}
static void bootstrap_server(int ready, int finish, int workers) {
    shared_fd=open("/dev/mini_binder",O_RDWR); assert(shared_fd>=0);
    unsigned char *mapping=map_fd(shared_fd);
    assert(ioctl(shared_fd,MINI_BINDER_REGISTER_MANAGER)==0);
    send_action(shared_fd,MINI_BC_ENTER_LOOPER); byte_out(ready);
    struct mini_binder_command event={0}, reply={.cmd=MINI_BC_REPLY};
    assert(read_cmd(shared_fd,&event)==0);
    reply.transaction.size=24; reply.transaction.object_count=2;
    for(int i=0;i<2;i++) {
        reply.transaction.object_offsets[i]=i*12;
        put32(reply.transaction.data+i*12,MINI_BINDER_OBJECT_LOCAL);
        put32(reply.transaction.data+i*12+4,101+i);
    }
    assert(write_cmd(shared_fd,&reply)==0);
    if(workers==2) {
        byte_in(finish);
        assert(read_cmd(shared_fd,(void *)(uintptr_t)1)==-1 && errno==EFAULT);
        assert(read_cmd(shared_fd,&event)==0 && event.transaction.flags==MINI_TF_ONE_WAY);
        assert(get32((const void *)(uintptr_t)event.transaction.data_address)==2);
        send_action(shared_fd,MINI_BC_FREE_BUFFER); byte_out(ready);
    } else if(workers) {
        pthread_t t[2];
        for(int i=0;i<2;i++) assert(pthread_create(&t[i],NULL,worker,(void *)(intptr_t)i)==0);
        for(int i=0;i<2;i++) assert(pthread_join(t[i],NULL)==0);
    }
    byte_in(finish);
    assert(munmap(mapping,(size_t)sysconf(_SC_PAGESIZE))==0); close(shared_fd); _exit(0);
}
static int connect_client(unsigned char **mapping) {
    int fd=open("/dev/mini_binder",O_RDWR); assert(fd>=0); *mapping=map_fd(fd);
    struct mini_binder_command c={.cmd=MINI_BC_TRANSACTION},event={0};
    assert(write_cmd(fd,&c)==0 && read_cmd(fd,&event)==0 && event.cmd==MINI_BR_REPLY);
    const unsigned char *p=*mapping+event.transaction.reply_offset;
    assert(get32(p)==MINI_BINDER_OBJECT_HANDLE && get32(p+4)==1 && get32(p+16)==2);
    send_action(fd,MINI_BC_FREE_REPLY); return fd;
}
static int submit(int fd,uint32_t handle,uint32_t value,uint32_t flags) {
    struct mini_binder_command c={.cmd=MINI_BC_TRANSACTION};
    c.transaction.target_handle=handle; c.transaction.flags=flags;
    c.transaction.code=9; c.transaction.size=4; put32(c.transaction.data,value);
    int payload[2]={-1,-1};
    if(value==7) {
        assert(pipe(payload)==0 && write(payload[1],"X",1)==1); close(payload[1]);
        c.transaction.object_count=1; c.transaction.object_offsets[0]=4; c.transaction.size=16;
        put32(c.transaction.data+4,MINI_BINDER_OBJECT_FD); put32(c.transaction.data+8,payload[0]);
    }
    int ret=write_cmd(fd,&c), saved=errno;
    if(payload[0]>=0) close(payload[0]);
    errno=saved; return ret;
}
static void wait_ok(pid_t pid) {
    int status; assert(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
}
int main(int argc, char **argv) {
    demo=argc==2 && !strcmp(argv[1],"--demo");
    assert(argc==1 || demo);
    alarm(30);
    int ready[2],finish[2]; assert(pipe(ready)==0 && pipe(finish)==0);
    for(int i=0;i<2;i++) assert(pipe(gates[i])==0 && pipe(releases[i])==0 && pipe(events[i])==0);
    pid_t server=fork(); assert(server>=0);
    if(!server) bootstrap_server(ready[1],finish[0],1);
    byte_in(ready[0]); unsigned char *mapping; int fd=connect_client(&mapping);
    if(!demo) {
        assert(submit(fd,1,0,2)==-1 && errno==EINVAL);
        assert(submit(fd,0,0,MINI_TF_ONE_WAY)==-1 && errno==EINVAL);
    }
    for(int round=0;round<(demo?1:6);round++) {
        int same=round<5;
        assert(submit(fd,1,round*2+1,MINI_TF_ONE_WAY)==0);
        assert(submit(fd,same?1:2,round*2+2,MINI_TF_ONE_WAY)==0);
        if(!demo) assert(submit(fd,1,999,MINI_TF_ONE_WAY)==-1 && errno==EBUSY);
        struct mini_binder_command c={.cmd=MINI_BC_FREE_REPLY};
        if(!demo) assert(write_cmd(fd,&c)==-1 && errno==ENOENT);
        /* No worker has been allowed to read: both sends already returned. */
        byte_out(gates[0][1]); observe(events[0][0]);
        struct observation first=observe(events[0][0]);
        assert(first.cookie==101 && first.value==(uint32_t)(round*2+1));
        byte_out(gates[1][1]); struct observation started=observe(events[1][0]);
        if(same) {
            await_sleeping_ioctl(server,started.tid);
            byte_out(releases[0][1]); observe(events[0][0]);
        }
        struct observation second=observe(events[1][0]);
        assert(second.cookie==(same?101U:102U) && second.value==(uint32_t)(round*2+2));
        if(!same) {
            /* Both Nodes are being handled; close the sender before either completes. */
            assert(munmap(mapping,(size_t)sysconf(_SC_PAGESIZE))==0); close(fd);
            byte_out(releases[0][1]); observe(events[0][0]);
        }
        byte_out(releases[1][1]); observe(events[1][0]);
        if(!round) puts("same Node: second worker slept until first FREE_BUFFER; FIFO 1 -> 2");
    }
    if(demo) { munmap(mapping,(size_t)sysconf(_SC_PAGESIZE)); close(fd); }
    byte_out(finish[1]); wait_ok(server);
    if(demo) { puts("two notifications submitted before workers ran; both completed without BC_REPLY"); return 0; }
    puts("submission returned before processing; 12 slots reused; different Nodes ran concurrently; sender close passed");
    puts("FD request survived sender close; service THREAD_EXIT advanced the Node queue");
    /* Receiver close must discard both active and same-Node queued work. Repeat past capacity. */
    for(int round=0;round<6;round++) {
        assert(pipe(ready)==0 && pipe(finish)==0); server=fork(); assert(server>=0);
        if(!server) bootstrap_server(ready[1],finish[0],0);
        byte_in(ready[0]); fd=connect_client(&mapping);
        assert(submit(fd,1,1,MINI_TF_ONE_WAY)==0 && submit(fd,1,2,MINI_TF_ONE_WAY)==0);
        byte_out(finish[1]); wait_ok(server);
        assert(submit(fd,1,3,MINI_TF_ONE_WAY)==-1 && errno==ENOENT);
        munmap(mapping,(size_t)sysconf(_SC_PAGESIZE)); close(fd);
        close(ready[0]); close(ready[1]); close(finish[0]); close(finish[1]);
    }
    assert(pipe(ready)==0 && pipe(finish)==0); server=fork(); assert(server>=0);
    if(!server) bootstrap_server(ready[1],finish[0],2);
    byte_in(ready[0]); fd=connect_client(&mapping);
    assert(submit(fd,1,1,MINI_TF_ONE_WAY)==0 && submit(fd,1,2,MINI_TF_ONE_WAY)==0);
    byte_out(finish[1]); byte_in(ready[0]); byte_out(finish[1]); wait_ok(server);
    munmap(mapping,(size_t)sysconf(_SC_PAGESIZE)); close(fd);
    puts("failed request delivery reclaimed the first slot and advanced its Node queue");
    puts("receiver exit reclaimed active and queued oneway work; invalid flags/reply/free and capacity checks passed");
    return 0;
}
