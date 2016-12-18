#define _WITH_GETLINE
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/wait.h>

int sockfd;

void koniec(int sygnal) {
    wait(0);
    close(sockfd);
    exit(-1); 
}

int main(int argc, char **argv) {
    int i, pid, poczatek=1;
    size_t n=0;
    ssize_t rozmiar;
    char  *buf=NULL, dane[255];
    struct hostent *nazwa_hosta;
    struct in_addr *adres_hosta;
    struct sockaddr_in adres;
    struct sockaddr_in adres_nadawcy;
    struct sigaction sa_int;
    socklen_t addrlen = sizeof(adres_nadawcy);

    if(argc != 2) {
        printf("Program wymaga adresu hosta jako argument.\n");
        exit(-1);
    }

    /* Obsługa sygnału SIGINT */
    sa_int.sa_handler = koniec;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, 0);

    /* Tworzenie gniazda dla IPv4/UDP */
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Tworzenie gniazda");
        exit(-1);
    }
    
    /* Ustawienie adresu */
    bzero(&adres, sizeof(adres));
    adres.sin_port = htons(8945);
    adres.sin_family = AF_INET;

    /* Tworzenie procesu potomnego */
    if((pid = fork()) == -1) {
        perror("Tworzenie procesu potomnego");
        close(sockfd);
        exit(-1);
    }
    
    /* Proces macierzysty wysyła wiadomości */
    if(pid > 0) {
        /* Pobranie adresu hosta na podstawie nazwy */
        if((nazwa_hosta = gethostbyname(argv[1])) == NULL){
            perror("gethostbyname");
            kill(pid, SIGINT);
            koniec(SIGINT);
        }
        adres_hosta = (struct in_addr *)nazwa_hosta->h_addr;

        /* Ustawienie adresu odbiorcy */
        if((adres.sin_addr.s_addr = inet_addr(inet_ntoa(*adres_hosta))) == -1) {
            perror("Ustawianie adresu odbiorcy");
            kill(pid, SIGINT);
            koniec(SIGINT);
        }
        
        while(1) {
            /* Pobieranie wiadomomsci od uzytkownika */
            if((rozmiar = getline(&buf, &n, stdin)) == -1) {
                perror("Pobieranie wiadomości");
                kill(pid, SIGINT);
                koniec(SIGINT);
            }
            
            /* Warunek kończący działanie programu */
            if(strcmp(buf, "koniec\n") == 0) {
                kill(pid, SIGINT);
                wait(0);
                close(sockfd);
                exit(0);
            }


            /*  Wysyłanie wiadomości w częściach po 255 znaków każda */
            for(i=0; i<rozmiar; i=i+255) {
                bzero(dane, sizeof(dane));              /* Czyszczenie bufora */
                
                /* Kopiowanie maksymalnie 255 znakow do przesyłanej tablicy */
                if(strncpy(dane, buf, sizeof(dane)) == NULL) {
                    perror("Kopiowanie danych");
                    kill(pid, SIGINT);
                    koniec(SIGINT); 
                };     
                
                /* Wysłanie części wiadomości */
                if(sendto(sockfd, dane, sizeof(dane), 0, (struct sockaddr *)&adres, sizeof(adres)) == -1) {
                    perror("Wysyłanie wiadomości");
                    kill(pid, SIGINT);
                    koniec(SIGINT);
                }
                buf += 255;   /* Przesunięcie początku bufora */
            }
        }
    }

    /* Potomek odbiera wiadomości */
    else {
        /* Odbieranie wiadomości od każdego hosta */         
        adres.sin_addr.s_addr = htonl(INADDR_ANY);

        /* Przywiązanie nazwy do gniazda */
        if(bind(sockfd, (struct sockaddr *)&adres, sizeof(adres)) == -1) {
            perror("Podpinanie gniazda");
            kill(pid, SIGINT);
            exit(-1); 
        }
        
        while(1) {
            /* Odbieranie wiadomości */
            if(recvfrom(sockfd, &dane, sizeof(dane), 0, (struct sockaddr *)&adres_nadawcy, &addrlen) == -1) {
                perror("Odbieranie wiadomości");
                kill(pid, SIGINT);
                exit(-1);
            }
        
            /* Jeśli odebrano pierwszą część wiadomości to wyświetl adres nadawcy */
            if(poczatek == 1) {
                printf("[%s] ", inet_ntoa(adres_nadawcy.sin_addr));
                poczatek = 0;
            }

            printf("%s", dane);     /* Drukowanie części wiadomości */
 
            /* Znak nowego wiersza oznacza koniec wiadomości */
            if(dane[strlen(dane)-1] == '\n')
                poczatek = 1;
           
            /* Czyszczenie buforów */
            bzero(&adres_nadawcy, sizeof(adres_nadawcy));
            bzero(&dane, sizeof(dane));
        }
    }
}
