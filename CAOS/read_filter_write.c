/*
Принимает в качестве аргументов командной строки 3 имени файла,
читает из первого, во второй записывает цифры, в третий всё остальное
*/


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>
#include <malloc.h>

static const size_t BSIZE = 4096;

static void
clean_up_and_exit(int ret_code);

typedef struct {
    int fd_;
    size_t size_, current_pos_;
    char * buff_;
} ifstream_t;

static void
init_ifstream(ifstream_t * ifstream, const char * file_name)
{
    ifstream->fd_ = -1;
    ifstream->buff_ = NULL;
    // Эти значения используются при очистке, если что-то пойдёт не так
    // ,то нужно определить, был ли открыт файл и была ли выделена память
    ifstream->fd_ = open(file_name, O_RDONLY);
    if (ifstream->fd_ == -1)
        clean_up_and_exit(1);
    ifstream->buff_ = malloc(BSIZE);
    if (ifstream->buff_ == NULL)
    {
        close(ifstream->fd_);
        clean_up_and_exit(3);
    }

    ifstream->size_ = ifstream->current_pos_ = 0;
}

static void
release_ifstream(ifstream_t * ifstream)
{
    if (ifstream->fd_ >= 0)
        close(ifstream->fd_);
    free(ifstream->buff_);
}

static bool
eof(ifstream_t * ifstream)
{
    if (ifstream->current_pos_ == ifstream->size_)
    {
        ssize_t read_chars = read(ifstream->fd_, ifstream->buff_, BSIZE);
        if (-1 == read_chars)
            clean_up_and_exit(3);
        if (0 == read_chars)
            return true;
        ifstream->size_ = (size_t) read_chars;
        ifstream->current_pos_ = 0;
        return false;
    }
    else
        return false;
}

static char
read_char(ifstream_t * ifstream)
{
    if (ifstream->current_pos_ < ifstream->size_)
        return ifstream->buff_[ifstream->current_pos_++];
    ssize_t read_chars = read(ifstream->fd_, ifstream->buff_, BSIZE);
    if (0 >= read_chars)
        clean_up_and_exit(3); // Ну нельзя читать без проверки на eof!
    ifstream->size_ = (size_t) read_chars;
    ifstream->current_pos_ = 0;
    return ifstream->buff_[ifstream->current_pos_++];
}

ifstream_t input;

typedef struct {
    int fd_;
    size_t size_;
    char * buff_;
} ofstream_t;

static void
init_ofstream(ofstream_t * ofstream, const char * file_name)
{
    ofstream->fd_ = -1;
    ofstream->buff_ = NULL;

    ofstream->fd_ = open(file_name, O_WRONLY | O_CREAT, 0640);
    if (-1 == ofstream->fd_)
        clean_up_and_exit(2);
    ofstream->buff_ = malloc(BSIZE);
    if (NULL == ofstream->buff_)
    {
        close(ofstream->fd_);
        clean_up_and_exit(3);
    }

    ofstream->size_ = 0;
}

static void
flush_ofstream(ofstream_t * ofstream)
{
    if (ofstream->size_ > 0)
    {
        size_t writen = 0;
        while (writen != ofstream->size_)
        {
            ssize_t writen_char = write(ofstream->fd_,
                    ofstream->buff_ + writen, ofstream->size_ - writen);
            if (-1 == writen_char)
                clean_up_and_exit(2);
            writen += writen_char;
        }
        ofstream->size_ = 0;
    }
}

static void
write_char(ofstream_t * ofstream, char ch)
{
    if (ofstream->size_ == BSIZE)
        flush_ofstream(ofstream);
    ofstream->buff_[ofstream->size_++] = ch;
}

static void
release_ofstream(ofstream_t * ofstream)
{
    if (ofstream->fd_ != -1 && ofstream->buff_ != NULL)
        flush_ofstream(ofstream);
    if (ofstream->fd_ != -1)
        close(ofstream->fd_);
    free(ofstream->buff_);
}

ofstream_t digit_output, other_output;

static void
clean_up_and_exit(int ret_code)
{
    release_ifstream(&input);
    release_ofstream(&digit_output);
    release_ofstream(&other_output);
    _exit(ret_code);
}

static void
init_program(const char * file1, const char * file2, const char * file3)
{
    init_ifstream(&input, file1);
    init_ofstream(&digit_output, file2);
    init_ofstream(&other_output, file3);
}

static void
run_program()
{
    while (!eof(&input))
    {
        char c = read_char(&input);
        if ('0' <= c && c <= '9')
            write_char(&digit_output, c);
        else
            write_char(&other_output, c);
    }
}

int
main(int argc, char ** argv)
{
    init_program(argv[1], argv[2], argv[3]);
    run_program();
    clean_up_and_exit(0);
}
