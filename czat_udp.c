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

void obsluga(int sygnal) {
    close(sockfd);
    wait(0);
    exit(0); 
}

void obs_usr1(int sygnal) {
    int s_time;
    s_time = sleep(2);
    if(s_time == 0)
        printf("[BŁĄD]\n");
}

int main(int argc, char **argv) {
    int pid;
    char tresc[255];
    struct hostent *nazwa_hosta;
    struct in_addr *adres_hosta;
    struct sockaddr_in adres;
    struct sockaddr_in adres_nadawcy;
    struct sigaction sa_int;
    struct sigaction sa_usr1;
    socklen_t addrlen = sizeof(adres_nadawcy);

    if(argc != 2) {
        printf("Program wymaga adresu hosta jako argument.\n");
        exit(-1);
    }

    /* Obsługa sygnału SIGINT */
    sa_int.sa_handler = obsluga;
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
        exit(-1);
    }
    
    /* Proces macierzysty wysyła wiadomości */
    if(pid > 0) {
        /* Pobranie adresu hosta na podstawie nazwy */
        if((nazwa_hosta = gethostbyname(argv[1])) == NULL){
            perror("gethostbyname");
            kill(pid, SIGINT);
            exit(-1);
        }
        adres_hosta = (struct in_addr *)nazwa_hosta->h_addr;

        /* Ustawienie adresu odbiorcy */
        if((adres.sin_addr.s_addr = inet_addr(inet_ntoa(*adres_hosta))) == -1) {
            perror("Ustawianie adresu odbiorcy");
            kill(pid, SIGINT);
            exit(-1);
        }
        
        while(1) {
            bzero(tresc, sizeof(tresc));            /* Czyszczenie bufora */
            fgets(tresc, sizeof(tresc), stdin);     /* Pobieranie wiadomości od użytkownika */
            
            /* Warunek kończący działanie programu */
            if(strcmp(tresc, "koniec\n") == 0) {
                kill(pid, SIGINT);  /* Zakończ działanie potomka */
                obsluga(SIGINT);    /* Zakończ działanie procesu macierzystego */
            }

            /* Wysyłanie wiadomości */
            if(sendto(sockfd, tresc, sizeof(tresc), 0, (struct sockaddr *)&adres, sizeof(adres)) == -1) {
                perror("Wysyłanie wiadomości");
                kill(pid, SIGINT);
                exit(-1);
            }
            
            /* Wysłanie sygnału do potomka oznaczającego wysłanie wiadomości */
           /* kill(pid, SIGUSR1);*/
        }
    }

    /* Potomek odbiera wiadomości */
    else {
        /* Zmiana działania sygnału SIGUSR1 */
       /* sa_usr1.sa_flags = 0;
        sigemptyset(&sa_usr1.sa_mask);
        sa_usr1.sa_handler = obs_usr1;*/

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
            if(recvfrom(sockfd, &tresc, sizeof(tresc), 0, (struct sockaddr *)&adres_nadawcy, &addrlen) == -1) {
                perror("Odbieranie wiadomości");
                kill(pid, SIGINT);
                exit(-1);
            }
        
            /* Drukowanie wiadomości */
            printf("[%s] %s", inet_ntoa(adres_nadawcy.sin_addr), tresc);
           
            /* ############################################################################################ */
            /* Wysyłanie wiadomości */
            if(sendto(sockfd, "[OK]", sizeof("[OK]"), 0, (struct sockaddr *)&adres_nadawcy, sizeof(adres_nadawcy)) == -1) {
                perror("Wysyłanie wiadomości");
                kill(pid, SIGINT);
                exit(-1);
            }
 
            /* ########################################################################################### */
            /* Czyszczenie buforów */
            bzero(&adres_nadawcy, sizeof(adres_nadawcy));
            bzero(&tresc, sizeof(tresc));
        }
    }
}
