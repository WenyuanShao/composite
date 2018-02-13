#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <posix.h>
#include <rumpcalls.h>
#include <vk_types.h>
#include <shdmem.h>
#include <llprint.h>
#include <rk.h>

extern int vmid;

/*
 * Essentially everything in this file can be deleted or except for the packing code for
 * a few of the invocations. This file is used on the application side to invoke into the Rumpkernel
 */

int
rk_inv_op1(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP1, 0, 0, 0);
}

void
rk_inv_op2(int shmid)
{
	cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP2, shmid, 0, 0);
}

int
rk_inv_get_boot_done(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_GET_BOOT_DONE, 0, 0, 0);
}

int
rk_inv_socket(int domain, int type, int protocol)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_SOCKET, domain, type, protocol);
}

int
rk_inv_bind(int sockfd, int shdmem_id, socklen_t addrlen)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_BIND, sockfd, shdmem_id, addrlen);
}

ssize_t
rk_inv_recvfrom(int s, int buff_shdmem_id, size_t len, int flags, int from_shdmem_id, int fromlenaddr_shdmem_id)
{
	assert(s <= 0xFFFF);
	assert(buff_shdmem_id <= 0xFFFF);
	assert(len <= 0xFFFF);
	assert(flags <= 0xFFFF);
	assert(from_shdmem_id <= 0xFFFF);
	assert(fromlenaddr_shdmem_id <= 0xFFFF);

	return (ssize_t)cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_RECVFROM,
			(s << 16) | buff_shdmem_id, (len << 16) | flags,
			(from_shdmem_id << 16) | fromlenaddr_shdmem_id);
}

ssize_t
rk_inv_sendto(int sockfd, int buff_shdmem_id, size_t len, int flags, int addr_shdmem_id, socklen_t addrlen)
{
	assert(sockfd <= 0xFFFF);
	assert(buff_shdmem_id <= 0xFFFF);
	assert(len <= 0xFFFF);
	assert(flags <= 0xFFFF);
	assert(addr_shdmem_id <= (int)0xFFFF);
	assert(addrlen <= (int)0xFFFF);

	return (ssize_t)cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_SENDTO,
			(sockfd << 16) | buff_shdmem_id, (len << 16) | flags,
			(addr_shdmem_id << 16) | addrlen);
}

/* still using ringbuffer shared data */
int
rk_inv_logdata(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_LOGDATA, 0, 0, 0);
}

int
rk_socketcall(int call, unsigned long *args)
{
        int ret = -1;
	/*
	 * Set and unset by sendto and recvfrom to ensure that only 1 thread
	 * is sending and recieving packets. This means that we will never have
	 * have more than 1 packet in the send or recv shdmem page at a given time
	 */
	static int canSend = 0;

        switch (call) {
		case 1: { /* Socket */
                        int domain, type, protocol;

                        domain     = *args;
                        type       = *(args + 1);
                        protocol   = *(args + 2);
                        ret = rk_inv_socket(domain, type, protocol);

                        break;
                }
                case 2: { /* Bind */
                        int sockfd, shdmem_id;
                        vaddr_t shdmem_addr;
                        void *addr;
                        u32_t addrlen;

                        sockfd  = *args;
                        addr    = (void *)*(args + 1);
                        addrlen = *(args + 2);

                        /*
                         * Do stupid shared memory for now
                         * allocate a page for each bind addr
                         * don't deallocate. #memLeaksEverywhere
			 * We don't have to fix this for now as we only have 1 bind
                         */

			/* TODO make this a function */
			/* FIXME, shm_allocate should not take 0,0 */
                        shdmem_id = shm_allocate(0, 0);
                        assert(shdmem_id > -1);
                        shdmem_addr = shm_get_vaddr(0, shdmem_id);
                        assert(shdmem_addr > 0);

                        memcpy((void * __restrict__)shdmem_addr, addr, addrlen);
                        ret = rk_inv_bind(sockfd, shdmem_id, addrlen);

                        break;
                }
		case 11: { /* sendto */
			int fd, flags;
			static int shdmem_id = -1;
			static vaddr_t shdmem_addr = 0;
			vaddr_t shdmem_addr_tmp;
			const void *buff;
			void *shdmem_buff;
			size_t len;
			const struct sockaddr *addr;
			struct sockaddr *shdmem_sockaddr;
			socklen_t addrlen;

			fd      = (int)*args;
			buff    = (const void *)*(args + 1);
			len     = (size_t)*(args + 2);
			flags   = (int)*(args + 3);
			addr    = (const struct sockaddr *)*(args + 4);
			addrlen = (socklen_t)*(args + 5);

			/* TODO make this a function */
			if (shdmem_id < 0 && !shdmem_addr) {
				/* FIXME */
				shdmem_id = shm_allocate(0, 0);
				shdmem_addr = shm_get_vaddr(0, shdmem_id);
			}

			assert(shdmem_id > -1);
			assert(shdmem_addr > 0);

			assert(canSend == 1);
			canSend = 0;

			shdmem_addr_tmp = shdmem_addr;
			shdmem_buff = (void *)shdmem_addr_tmp;
			memcpy(shdmem_buff, buff, len);
			shdmem_addr_tmp += len;

			shdmem_sockaddr = (struct sockaddr*)shdmem_addr_tmp;
			memcpy(shdmem_sockaddr, addr, addrlen);

			ret = (int)rk_inv_sendto(fd, shdmem_id, len, flags,
				shdmem_id, addrlen);

			break;
		}
		case 12: { /* Recvfrom */
                        int s, flags;
			static int shdmem_id = -1;
			static vaddr_t shdmem_addr = 0;
			vaddr_t shdmem_addr_tmp;
                        void *buff;
                        size_t len;
                        struct sockaddr *from_addr;
                        u32_t *from_addr_len_ptr;

                        s                 = *args;
                        buff              = (void *)*(args + 1);
                        len               = *(args + 2);
                        flags             = *(args + 3);
                        from_addr         = (struct sockaddr *)*(args + 4);
                        from_addr_len_ptr = (u32_t *)*(args + 5);

			/* TODO make this a function */
			if (shdmem_id < 0 && !shdmem_addr) {
				/* FIXME */
				shdmem_id = shm_allocate(0, 0);
				shdmem_addr = shm_get_vaddr(0, shdmem_id);
			}

                        assert(shdmem_id > -1);
                        assert(shdmem_addr > 0);

			assert(canSend == 0);
			canSend = 1;

                        ret = (int)rk_inv_recvfrom(s, shdmem_id, len, flags,
                                shdmem_id, *from_addr_len_ptr);

                        /* TODO, put this in a function */
                        /* Copy buffer back to its original value*/
			shdmem_addr_tmp = shdmem_addr;
                        memcpy(buff, (const void * __restrict__)shdmem_addr_tmp, ret);
                        shdmem_addr_tmp += len; /* Add overall length of buffer */

                        /* Set from_addr_len_ptr pointer to be shared memory at right offset */
                        *from_addr_len_ptr = *(u32_t *)shdmem_addr_tmp;
                        shdmem_addr_tmp += sizeof(u32_t *);

                        /* Copy from_addr to be shared memory at right offset */
                        memcpy(from_addr, (const void * __restrict__)shdmem_addr_tmp, *from_addr_len_ptr);

                        break;
                }
                default:
                        assert(0);
        }

        return ret;
}

int
rk_socketcall_init(void)
{
	assert(vmid != 0);

	/*
	 * Should only need this if a libc application is booted from the RK,
	 * it is currently not, it is booted by the llbooter
	 */

	//posix_syscall_override((cos_syscall_t)rk_socketcall, __NR_socketcall);

	return 0;
}
