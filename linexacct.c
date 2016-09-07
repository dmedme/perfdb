/*
 * Read a Solaris Extended Accounting format file. Simple version does not
 * cater for recursion.
 */
#include <stdio.h>
#include <time.h>
#include "e2conv.h"
struct	proc_acct
{
    int   pac_flag;    /* Accounting flag */
    int   pac_pid;    /* Pid */
    int   pac_uid;     /* Accounting user ID */
    int   pac_gid;     /* Accounting group ID */
    int   pac_tty_maj;     /* control tty */
    int   pac_tty_min;     /* control tty */
    long int pac_btime_secs;   /* Beginning time */
    long int pac_btime_nsecs;   /* Beginning time */
    long int pac_etime_secs;
    long int pac_etime_nsecs;
    long int pac_utime_secs;
    long int pac_utime_nsecs;
    long int pac_stime_secs;
    long int pac_stime_nsecs;
    char pac_comm[16]; /* command name */
} pac;
/***************************************************************************
 * Time/Nanosec arithmetic function
 *
 * t3 = t1 - t2
 */
static void tndiff(t1, n1, t2, n2, t3, n3)
long * t1;
long * n1;
long * t2;
long * n2;
long * t3;
long * n3;
{
    *t3 = *t1 - *t2;
    *n3 = *n1 - *n2;
    if (*n3 < 0)
    {
        *n3 = (*n3) + 1000000000;
        (*t3)--;
    }
    return;
}
static void date_out(obuf, secs, nsecs)
char * obuf;
long int secs;
long int nsecs;
{
time_t t = (time_t) secs;
char buf[32];
#ifdef SOLAR
char * x = ctime_r(&t, buf, sizeof(buf));
#else
char * x = ctime_r(&t, buf);
#endif

    if (x != (char *) NULL)
        sprintf(obuf, "%2.2s %3.3s %4.4s %8.8s.%09lu",
            (x + 8), (x + 4), (x + 20), (x + 11),
            nsecs);
    return;
}
int main(argc, argv)
int argc;
char **argv;
{
int c;
long int xl;
long int xln;
int el_cnt;
union {
    unsigned int ui[4];
    unsigned char buf[16];
} wbuf;
unsigned char * buf;
unsigned char * bound;
unsigned char * x;

    memset((unsigned char *) &wbuf, 0, sizeof(wbuf));
    while ((c = fgetc(stdin)) != EOF)
    {
        if (c == 0xf0)
        {   /* Group */
            wbuf.buf[0] = (unsigned int) c;
            fread(&wbuf.buf[1],1,3,stdin);
            other_end(&wbuf.buf[0],4);
            if (wbuf.ui[0] !=  0xf0000100)
            { /* Not an accounting record - skip */
                fread(&wbuf.buf[0],1,8,stdin);
                other_end(&wbuf.buf[0],8); /* Don't bother to check the trailer */
                fseek(stdin, (((unsigned long int)wbuf.ui[1]) << 32) +
                              (unsigned long int)wbuf.ui[0] + 4, 1);
            }
            else
            {
                fread(&wbuf.buf[0],1,8,stdin);
                other_end(&wbuf.buf[0],8); /* Don't bother checking trailer */
                xl = (((unsigned long int)wbuf.ui[1]) << 32) +
                              (unsigned long int)wbuf.ui[0] + 4;
                buf = (unsigned char *) malloc(xl);
                bound =  buf + xl;
                fread(buf, xl , 1, stdin);
                memcpy((unsigned char *) &el_cnt, buf, sizeof(el_cnt));
                other_end((unsigned char *) &el_cnt, sizeof(el_cnt));
                for (x = &buf[8]; x < bound && el_cnt > 0; el_cnt-- )
                {
                    memcpy(&wbuf.buf[0],x,4);
                    other_end(&wbuf.buf[0],4);
                    switch(*x)
                    {
                    case 0x40:    /* 64 bit value */
                        memcpy(&wbuf.buf[4],x + 4,8);
                        other_end(&wbuf.buf[4],8);
                        xl = (((unsigned long int)wbuf.ui[2]) << 32) +
                              (unsigned long int)wbuf.ui[1];
                        switch(wbuf.ui[0])
                        {
                        case 0x4000100b: /* CPU User Sec */
                            pac.pac_utime_secs = xl;
                            break;
                        case 0x4000100c:  /* CPU User NSec */
                            pac.pac_utime_nsecs = xl;
                            break;
                        case 0x4000100d:  /* CPU Sys Sec */
                            pac.pac_stime_secs = xl;
                            break;
                        case 0x4000100e:  /* CPU Sys NSec */
                            pac.pac_stime_nsecs = xl;
                            break;
                        case 0x40001007:  /* Start Time Sec */
                            pac.pac_btime_secs = xl;
                            break;
                        case 0x40001008:  /* Start Time NSec */
                            pac.pac_btime_nsecs = xl;
                            break;
                        case 0x40001009:  /* End Time Sec */
                            pac.pac_etime_secs = xl;
                            break;
                        case 0x4000100a:  /* End Time NSec */
                            pac.pac_etime_nsecs = xl;
                            break;
                        default:
                            break;
                        }
                        x += 16;
                        break;
                    case 0x30:    /* 32 bit value */
                        memcpy(&wbuf.buf[4],x + 4,4);
                        other_end(&wbuf.buf[4],4);
                        c = (int)wbuf.ui[1];
                        switch(wbuf.ui[0])
                        {
                        case 0x30001000: /* PID */
                            pac.pac_pid = c;
                            break;
                        case 0x30001001:  /* User ID */
                            pac.pac_uid = c;
                            break;
                        case 0x30001002:  /* Group ID */
                            pac.pac_gid = c;
                            break;
                        case 0x3000100f:  /* CPU Sys NSec */
                            pac.pac_tty_maj = c;
                            break;
                        case 0x30001010:  /* Start Time Sec */
                            pac.pac_tty_min = c;
                            break;
                        case 0x3000101d:  /* Start Time NSec */
                            pac.pac_flag = c;
                            break;
                        default:
                            break;
                        }
                        x += 12;
                        break;
                    case 0x60:    /* String */
                        if (wbuf.ui[0] == 0x60001006)
                        {
                            x += 12;
                            c = strlen(x) + 1;
                            memcpy(&pac.pac_comm, x,
                                   (c   > sizeof(pac.pac_comm))?
                                           sizeof(pac.pac_comm) : c);
                            x += c + 4;
                        }
                        else
                        {
                            x += 12;
                            x += strlen(x) + 5;
                        }
                        break;
                    default:
                        fprintf(stderr, "Oh dear ... %x ... should skip!\n", wbuf.ui[0]);
                        break;
                    }
                }
                tndiff(&pac.pac_etime_secs,
                       &pac.pac_etime_nsecs,
                       &pac.pac_btime_secs,
                       &pac.pac_btime_nsecs,
                       &xl,
                       &xln);
                date_out(&wbuf.buf[0],pac.pac_btime_secs,pac.pac_btime_nsecs);
printf("%s|%lu.%09lu|%lu.%09lu|%lu.%09lu|%s|%lu.%09lu|%lu.%09lu|%u|%u|%u|%d,%d|%x\n",
                    pac.pac_comm,         /* Accounting command name */
                    pac.pac_utime_secs,
                    pac.pac_utime_nsecs,
                    pac.pac_stime_secs,
                    pac.pac_stime_nsecs,
                    xl,
                    xln,
                    &wbuf.buf[0],          /* Beginning time      */
                    pac.pac_btime_secs,
                    pac.pac_btime_nsecs,
                    pac.pac_etime_secs,
                    pac.pac_etime_nsecs,
                    pac.pac_uid,          /* Accounting user ID  */
                    pac.pac_gid,          /* Accounting group ID */
                    pac.pac_pid,          /* Process ID */
                    pac.pac_tty_maj,      /* control typewriter (major)*/
                    pac.pac_tty_min,      /* control typewriter (minor)*/
                    pac.pac_flag);
                memset((unsigned char *) &pac, 0, sizeof(pac));
                free(buf);
            }
        }
    }
    exit(0);
}
