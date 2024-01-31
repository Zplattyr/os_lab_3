#include "sys/wait.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char **argv) {

  pthread_cond_t *condition;
  pthread_mutex_t *mutex;
  char *message;
  int des_cond, des_msg, des_mutex;

  des_mutex =
      shm_open("mutex_lock", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);

  if (des_mutex < 0) {
    perror("failure on shm_open on des_mutex");
    exit(1);
  }

  if (ftruncate(des_mutex, sizeof(pthread_mutex_t)) == -1) {
    perror("Error on ftruncate to sizeof pthread_cond_t\n");
    exit(-1);
  }

  mutex =
      (pthread_mutex_t *)mmap(NULL, sizeof(pthread_mutex_t),
                              PROT_READ | PROT_WRITE, MAP_SHARED, des_mutex, 0);

  if (mutex == MAP_FAILED) {
    perror("Error on mmap on mutex\n");
    exit(1);
  }

  des_cond =
      shm_open("condwrite", O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG);

  if (des_cond < 0) {
    perror("failure on shm_open on des_cond");
    exit(1);
  }

  if (ftruncate(des_cond, sizeof(pthread_cond_t)) == -1) {
    perror("Error on ftruncate to sizeof pthread_cond_t\n");
    exit(-1);
  }

  condition =
      (pthread_cond_t *)mmap(NULL, sizeof(pthread_cond_t),
                             PROT_READ | PROT_WRITE, MAP_SHARED, des_cond, 0);

  if (condition == MAP_FAILED) {
    perror("Error on mmap on condition\n");
    exit(1);
  }

  pthread_mutexattr_t mutexAttr;
  pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init(mutex, &mutexAttr);

  pthread_condattr_t condAttr;
  pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
  pthread_cond_init(condition, &condAttr);

  int shared_filename_fd =
      shm_open("shared_filename", O_CREAT | O_RDWR, S_IRWXU);
  int shared_float_fd = shm_open("shared_float", O_CREAT | O_RDWR, S_IRWXU);
  int shared_state_fd = shm_open("shared_char", O_CREAT | O_RDWR, S_IRWXU);
  if (ftruncate(shared_filename_fd, sizeof(int)) == -1) {
    perror("Error on ftruncate to sizeof shared_filename_fd\n");
    exit(-1);
  }
  if (ftruncate(shared_float_fd, sizeof(float)) == -1) {
    perror("Error on ftruncate to sizeof shared_float_fd\n");
    exit(-1);
  };
  if (ftruncate(shared_state_fd, sizeof(int)) == -1) {
    perror("Error on ftruncate to sizeof shared_state_fd\n");
    exit(-1);
  };

  char *shared_filename = mmap(NULL, sizeof(char) * 100, PROT_READ | PROT_WRITE,
                               MAP_SHARED, shared_filename_fd, 0);
  float *shared_float = mmap(NULL, sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED,
                         shared_float_fd, 0);
  int *shared_state = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                           MAP_SHARED, shared_state_fd, 0);
  *shared_state = 1;
  shared_filename[0] = 0;

  pid_t pid = fork();

  if (pid == -1) {
    perror("Error on fork\n");
    exit(0);
  }

  if (pid == 0) {

    while (shared_filename[0] == 0) {
      pthread_mutex_lock(mutex);
      pthread_cond_wait(condition, mutex);
      pthread_mutex_unlock(mutex);
    }

    int output = open(shared_filename, O_RDONLY, S_IRWXU);
    char c;
    float f;
    char num_buf[100];
    int i = 0;
    int k = 0;
    float sum = 0;
    while (read(output, &c, 1) == 1) {
        if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
            num_buf[i] = c;
            i+=1;
        }
        else {
            num_buf[i] = '\0';
            i = 0;
            f = atof(num_buf);
            sum += f;
            k+=1;
            if (k == 3) {
                pthread_mutex_lock(mutex);
                *shared_state = 2;
                *shared_float = sum;
                pthread_cond_wait(condition, mutex);
                pthread_mutex_unlock(mutex);
                k = 0;
                sum = 0;
            }
        }
    };
    *shared_state = 0;
    close(output);
  }

  else {

    scanf("%100s", shared_filename);
    pthread_cond_signal(condition);
    while (*shared_state != 0) {
      pthread_mutex_lock(mutex);
      if (*shared_state == 2) {
        printf("%f\n", *shared_float);
        *shared_state = 1;
        pthread_cond_signal(condition);
      }
      pthread_mutex_unlock(mutex);
    }
    pthread_condattr_destroy(&condAttr);
    pthread_mutexattr_destroy(&mutexAttr);
    pthread_mutex_destroy(mutex);
    pthread_cond_destroy(condition);

    wait(NULL);
  }

  return 0;
}