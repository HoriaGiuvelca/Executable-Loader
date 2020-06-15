/*
 * Loader Implementation
 *
 * 2018, Operating Systems
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "exec_parser.h"

static int fd_executable;
static so_exec_t *exec;

typedef struct pagini_mapate {
	int adresa_pagina;
	struct pagini_mapate *next;
} pagini_mapate; /*lista pe care o voi completa cu toate paginile mapate*/

static pagini_mapate *head;

static void init_lista(int adresa_pagina)
{
	head = malloc(sizeof(pagini_mapate));
	head->adresa_pagina = adresa_pagina;
	head->next = NULL;
}

void adaugare_element(int adresa_pagina)
{
	pagini_mapate *current = head;

	while (current->next != NULL)
		current = current->next;

	current->next = malloc(sizeof(pagini_mapate));
	current->next->adresa_pagina = adresa_pagina;
	current->next->next = NULL;
}

int cautare_element(int adresa_pagina)
{
	pagini_mapate *current = head;

	if (head->next == NULL) {
		if (head->adresa_pagina == adresa_pagina)
			return 1;
		else
			return 0;
	}

	while (current->next != NULL) {
		if (current->adresa_pagina == adresa_pagina)
			return 1;

		current = current->next;
	}

	if (current->next == NULL && current->adresa_pagina == adresa_pagina)
		return 1;

	return 0;
}

so_seg_t *cautare_segment_problema(void *addr_problema)
{
	int aux = 0; /*prin intermediul lui aux voi afla segmentul in care s-a produs eroarea*/

	for (int i = 0; i < exec->segments_no; i++) {
		aux = (char *)addr_problema - (char *)exec->segments[i].vaddr;

		if (aux >= 0 && aux < exec->segments[i].mem_size)
			return &(exec->segments[i]);
	}

	return NULL;
}

static void segv_handler(int sig, siginfo_t *si, void *degeaba)
{
	so_seg_t *segment_problema = NULL;
	size_t offset_in_segment = 0;
	size_t offset_in_pagina = 0;

	segment_problema = cautare_segment_problema(si->si_addr);

	/*diferenta dintre adresa la care s-a generat eroarea si vaddr*/
	offset_in_segment = (char *)si->si_addr -
	                    (char *)segment_problema->vaddr;

	/*diferenta dintre adresa erorii si adresa paginii cu eroarea*/
	offset_in_pagina = offset_in_segment % getpagesize();

	/*adresa inceputului paginii cu eroarea*/
	offset_in_segment -= offset_in_pagina;

	if (segment_problema != NULL) { /*a fost identificat segmentul cu eroare*/
		if (head == NULL) { /*prima pagina care trebuie mapata*/
			/*trebuie initializata lista*/
			init_lista(segment_problema->vaddr + offset_in_segment); 
			mmap((void *)segment_problema->vaddr + offset_in_segment,
			     getpagesize(), PERM_R | PERM_W,
			     MAP_ANONYMOUS | MAP_FIXED | MAP_PRIVATE, -1, 0);

			if (offset_in_segment + getpagesize()
			    <= segment_problema->file_size) { /*toata pagina se gaseste in fisier*/
				lseek(fd_executable, segment_problema->offset +
				      offset_in_segment, SEEK_SET);
				read(fd_executable, (void *)segment_problema->vaddr
				     + offset_in_segment, getpagesize());
				mprotect((void *)segment_problema->vaddr + offset_in_segment,
				         getpagesize(), segment_problema->perm);
			} else if (offset_in_segment <= segment_problema->file_size &&
			           offset_in_segment + getpagesize() > segment_problema->file_size) { /*o parte din pagina se gaseste in fisier, iar alta parte trebuie zeroizata*/
				lseek(fd_executable, segment_problema->offset +
				      offset_in_segment, SEEK_SET);
				read(fd_executable, (void *)segment_problema->vaddr +
				     offset_in_segment, segment_problema->file_size -
				     offset_in_segment);
				memset((void *)segment_problema->vaddr +
				       segment_problema->file_size, 0, getpagesize() -
				       segment_problema->file_size + offset_in_segment);
				mprotect((void *)segment_problema->vaddr + offset_in_segment,
				         getpagesize(), segment_problema->perm);
			} else { /*pagina nu se regaseste in fisier si trebuie zeroizata*/
				memset((void *)segment_problema->vaddr + offset_in_segment, 0,
				       getpagesize());
				mprotect((void *)segment_problema->vaddr + offset_in_segment,
				         getpagesize(), segment_problema->perm);
			}
		} else { /*capul listei este deja existent*/
			if (cautare_element(segment_problema->vaddr + offset_in_segment))
				/*pagina a fost deja mapata anterior, deci eroarea primita este "pe bune"*/
				exit(139);
			else { /*pagina nu a mai fost mapata anterior, deci o mapez acum*/
				adaugare_element(segment_problema->vaddr + offset_in_segment);
				mmap((void *)segment_problema->vaddr + offset_in_segment,
				     getpagesize(), PERM_R | PERM_W, MAP_ANONYMOUS | MAP_FIXED
				     | MAP_PRIVATE, -1, 0);

				if (offset_in_segment + getpagesize() <=
				    segment_problema->file_size) { /*toata pagina se gaseste in fisier*/
					lseek(fd_executable, segment_problema->offset +
					      offset_in_segment, SEEK_SET);
					read(fd_executable, (void *)segment_problema->vaddr +
					     offset_in_segment, getpagesize());
					mprotect((void *)segment_problema->vaddr +
					         offset_in_segment, getpagesize(), segment_problema->perm);
				} else if (offset_in_segment <= segment_problema->file_size &&
				           offset_in_segment + getpagesize() >
				           segment_problema->file_size) { /*o parte din pagina se gaseste in fisier, iar alta parte trebuie zeroizata*/
					lseek(fd_executable, segment_problema->offset +
					      offset_in_segment, SEEK_SET);
					read(fd_executable, (void *)segment_problema->vaddr +
					     offset_in_segment, segment_problema->file_size -
					     offset_in_segment);
					memset((void *)segment_problema->vaddr +
					       segment_problema->file_size, 0, getpagesize() -
					       segment_problema->file_size + offset_in_segment);
					mprotect((void *)segment_problema->vaddr +
					         offset_in_segment, getpagesize(), segment_problema->perm);
				} else { /*pagina nu se regaseste in fisier si trebuie zeroizata*/
					memset((void *)segment_problema->vaddr + offset_in_segment,
					       0, getpagesize());
					mprotect((void *)segment_problema->vaddr +
					         offset_in_segment, getpagesize(), segment_problema->perm);
				}
			}
		}
	} else /*segmentul accesat nu este unul cunoscut*/
		exit(139);
}

int so_init_loader(void)
{
	struct sigaction action;
	int rc;

	memset(&action, 0, sizeof(action));
	action.sa_sigaction = segv_handler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGSEGV);
	action.sa_flags = SA_SIGINFO;


	rc = sigaction(SIGSEGV, &action, NULL);

	if (rc == -1)
		perror("sigaction");

	return -1;
}

int so_execute(char *path, char *argv[])
{
	exec = so_parse_exec(path);

	if (!exec)
		return -1;

	fd_executable = open(path, O_RDONLY);
	so_start_exec(exec, argv);

	return -1;
}
