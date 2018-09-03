#include <mpi.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <limits.h>

//BOOL
typedef int bool;
enum
{
    false,
    true
};

/* CONST VALUES */
#define MESSAGE_COUNT 5
#define MSG_TAG 100

/* GLOBAL VARIABLES (ONLY FOR THREADS) */
int noMembers = 0; // N - NUMBER OF MEMBERS
int noClubs = 0;   // K - HOW MANY CLUBS
int entryCost = 0; // M - ENTRY AMOUNT
int memberMoney = 0;
int groupMoney = 0;
int memberId = 0;
int preferedClubId = 0;
int localClock = 0;
int approveCount = 0;
bool threadMoneyFlag = false;

/* ASK TAB VALUES */
int *askTab;
#define READY_ASK_TAB 0
#define ACCEPT_ASK_TAB 1
#define REJECT_ASK_TAB 2

/* MY STATUS VALUES */
int myStatus = 0;
#define ALONE_STATUS 0
#define LEADER_STATUS 1
#define MEMBER_STATUS 2
#define ACCEPT_INVITATION_STATUS 3
#define REJECT_INVITATION_STATUS 4
#define ENOUGH_MONEY_STATUS 5
#define ENTER_CLUB_STATUS 6
#define EXIT_CLUB_STATUS 7
#define GROUP_BREAK_STATUS 8

/* MESSAGE TYPE VALUES */
#define ASK_TO_JOIN_MSG 0
#define CONFIRM_JOIN_MSG 1
#define REJECT_JOIN_MSG 2
#define GROUP_BREAK_MSG 3
#define ASK_TO_ENTER_CLUB_MSG 4
#define AGREE_TO_ENTER_CLUB_MSG 5
#define DISAGREE_TO_ENTER_CLUB_MSG 6
#define EXIT_CLUB_MSG 7

//TYPE OF PACKAGE SENDING BETWEEN MEMBERS
MPI_Datatype mpiMsgType;

typedef struct msg_s
{
    int localClock;
    int message;
    int memberId;
    int preferedClubId;
    int memberMoney;
} msg;

int max(int a, int b)
{
    if (a > b)
        return a;
    else
        return b;
}

msg createPackage(int localClock, int message, int memberId, int preferedClubId, int memberMoney)
{
    msg package;

    package.localClock = localClock;
    package.message = message;
    package.memberId = memberId;
    package.preferedClubId = preferedClubId;
    package.memberMoney = memberMoney;
    return package;
}

void *threadFunc()
{
    msg sendMsg;
    msg recvMsg;

    while (true)
    {
        MPI_Recv(&recvMsg, 1, mpiMsgType, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        localClock = max(recvMsg.localClock, localClock) + 1;

        //2
        if (myStatus == ENOUGH_MONEY_STATUS && recvMsg.message == ASK_TO_ENTER_CLUB_MSG)
        {
            if (recvMsg.preferedClubId != preferedClubId)
            {
                localClock++;
                sendMsg = createPackage(localClock, AGREE_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
                MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
                printf("[myId: %d][clock: %d][to:   %d] Pozwolenie na wejscie do klubu o nr: %d {%d}\n", memberId, localClock, recvMsg.memberId, recvMsg.preferedClubId, myStatus);
            }
            else
            {
                if (recvMsg.localClock < localClock)
                {
                    localClock++;
                    sendMsg = createPackage(localClock, AGREE_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
                    MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
                    printf("[myId: %d][clock: %d][to:   %d] Pozwolenie na wejscie do klubu o nr: %d {%d}\n", memberId, localClock, recvMsg.memberId, recvMsg.preferedClubId, myStatus);
                }
            }
        }

        //3
        else if ((myStatus != ALONE_STATUS && myStatus != GROUP_BREAK_STATUS) && recvMsg.message == ASK_TO_JOIN_MSG)
        {
            localClock++;
            sendMsg = createPackage(localClock, REJECT_JOIN_MSG, memberId, preferedClubId, memberMoney);
            MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
            printf("[myId: %d][clock: %d][to:   %d] Odrzucenie zaproszenia do grupy {%d}\n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //4
        else if ((myStatus == LEADER_STATUS || myStatus == ALONE_STATUS) && recvMsg.message == CONFIRM_JOIN_MSG)
        {
            groupMoney += recvMsg.memberMoney;
            *(askTab + recvMsg.memberId) = ACCEPT_ASK_TAB;
            myStatus = ACCEPT_INVITATION_STATUS;
            ++approveCount;
            printf("[myId: %d][clock: %d][from: %d] Dołączył do grupy! Mamy teraz %d na %d pieniedzy {%d}\n", memberId, localClock, recvMsg.memberId, groupMoney, entryCost, myStatus);
        }

        //5
        else if (myStatus == LEADER_STATUS && recvMsg.message == REJECT_JOIN_MSG)
        {
            *(askTab + recvMsg.memberId) = REJECT_ASK_TAB;
            myStatus = REJECT_INVITATION_STATUS;
            printf("[myId: %d][clock: %d][from: %d] Odrzucenie propozycji dołączenia do grupy {%d}\n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //6
        else if (myStatus == ENOUGH_MONEY_STATUS && recvMsg.message == AGREE_TO_ENTER_CLUB_MSG)
        {
            approveCount++;
            printf("[myId: %d][clock: %d][from: %d] Pozwolenie dla mnie na wejscie do klubu %d {%d}\n", memberId, localClock, recvMsg.memberId, preferedClubId, myStatus);
            if (approveCount == noMembers - 1)
            {
                myStatus = ENTER_CLUB_STATUS;
            }
        }

        //7
        else if (myStatus != ENOUGH_MONEY_STATUS && myStatus != ENTER_CLUB_STATUS && recvMsg.message == ASK_TO_ENTER_CLUB_MSG)
        {
            localClock++;
            sendMsg = createPackage(localClock, AGREE_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
            MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
            printf("[myId: %d][clock: %d][to:   %d] Pozwolenie na wejscie do klubu o nr: %d {%d}\n", memberId, localClock, recvMsg.memberId, recvMsg.preferedClubId, myStatus);
        }

        //8
        else if ((myStatus == ALONE_STATUS || myStatus == GROUP_BREAK_STATUS) && recvMsg.message == ASK_TO_JOIN_MSG)
        {
            myStatus = MEMBER_STATUS;
            localClock++;
            sendMsg = createPackage(localClock, CONFIRM_JOIN_MSG, memberId, preferedClubId, memberMoney);
            MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
            printf("[myId: %d][clock: %d][to:   %d] Akceptuje zaproszenie do grupy {%d} \n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //9
        else if (myStatus == ALONE_STATUS && recvMsg.message == REJECT_JOIN_MSG)
        {
            myStatus = GROUP_BREAK_STATUS;
            *(askTab + recvMsg.memberId) = REJECT_ASK_TAB;
            printf("[myId: %d][clock: %d][from: %d] Moje zaproszenie zostalo odrzucone {%d}\n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //10
        else if (myStatus == MEMBER_STATUS && recvMsg.message == CONFIRM_JOIN_MSG)
        {
            localClock++;
            sendMsg = createPackage(localClock, GROUP_BREAK_MSG, memberId, preferedClubId, memberMoney);
            MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
            printf("[myId: %d][clock: %d][from: %d] Zrywam grupe (moje zaproszenie jest juz nie aktualne) {%d}\n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //11
        else if (myStatus == MEMBER_STATUS && recvMsg.message == GROUP_BREAK_MSG)
        {
            myStatus = GROUP_BREAK_STATUS;
            printf("[myId: %d][clock: %d][from: %d] Grupa zostala rozwiazana {%d}\n", memberId, localClock, recvMsg.memberId, myStatus);
        }

        //12
        else if (recvMsg.message == EXIT_CLUB_MSG)
        {
            preferedClubId = recvMsg.preferedClubId;
            printf("[myId: %d][clock: %d][from: %d] Wychodze z klubu jako czlonek grupy! Nr klubu: %d {%d}\n", memberId, localClock, recvMsg.memberId, preferedClubId, myStatus);
            myStatus = EXIT_CLUB_STATUS;
        }

        //13
        else if (myStatus == ENTER_CLUB_STATUS && recvMsg.message == ASK_TO_ENTER_CLUB_MSG)
        {
            if (recvMsg.preferedClubId != preferedClubId)
            {
                localClock++;
                sendMsg = createPackage(localClock, AGREE_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
                MPI_Send(&sendMsg, 1, mpiMsgType, recvMsg.memberId, MSG_TAG, MPI_COMM_WORLD);
                printf("[myId: %d][clock: %d][to:   %d] Pozwolenie na wejscie do klubu o nr: %d {%d}\n", memberId, localClock, recvMsg.memberId, recvMsg.preferedClubId, myStatus);
            }
        }
    }
    pthread_exit(NULL);
}

void initMember()
{
    if (threadMoneyFlag)
    {
        threadMoneyFlag = false;
    }
    else
    {
        memberMoney = (rand() % (entryCost - 2)) + 1;
        printf("[myId: %d][clock: %d]           my memberMoney = %d\n", memberId, localClock, memberMoney);
    }
    groupMoney = memberMoney;
    myStatus = ALONE_STATUS;
    preferedClubId = rand() % noClubs;
    askTab = calloc(noMembers, sizeof(int));
    approveCount = 0;

    //RESET askTab to 0 [READY_ASK_TAB]
    for (int i = 0; i < noMembers; i++)
    {
        askTab[i] = READY_ASK_TAB;
        if (i == memberId)
            askTab[i] = ACCEPT_ASK_TAB;
    }
}

bool isNotEmptyTab()
{
    for (int i = 0; i < noMembers; i++)
    {
        if (askTab[i] == 0)
        {
            return true;
        }
    }
    return false;
}

int getRandomMemberID()
{
    int val = rand() % noMembers;

    if (askTab[val] == READY_ASK_TAB)
    {
        if (val != memberId)
            return val;
    }

    for (int i = 0; i < noMembers; i++)
    {
        if (askTab[i] == READY_ASK_TAB)
        {
            if (i != memberId)
                return i;
        }
    }

    return -1;
}

void mainLoop()
{
    //START COMMUNICATE AFTER RANDOM SHORT TIME
    int sleepTime = rand() % (noMembers / 2);
    localClock += sleepTime;
    //sleep(localClock);
    //printf("[myId: %d][clock: %d]           Obudził się po %d czasu\n", memberId, localClock, sleepTime);

    msg sendMsg;

    //WHEN NEED TO RESET VALUES OF askTab etc.
    bool initAgain;
    //EXIT ASKING, ENOUGH MONEY TO ENTER CLUB OR WHEN GROUP BREAK
    bool exitWhile;

    while (true)
    {
        initMember();
        initAgain = false;

        while (isNotEmptyTab())
        {
            //LEADERS or ALONE STATUS
            if (myStatus != MEMBER_STATUS)
            {
                //INCREASE LAMPORT CLOCK
                localClock++;

                //SELECT MEMBER TO ASK FROM askTab
                int selectedMember = getRandomMemberID();
                if (selectedMember == -1)
                {
                    printf("[myId: %d] ERROR: Selected member == -1 {%d}\n", memberId, myStatus);
                    break;
                }

                //SEND MESSAGE TO SELECTED MEMBER
                sendMsg = createPackage(localClock, ASK_TO_JOIN_MSG, memberId, preferedClubId, memberMoney);
                MPI_Send(&sendMsg, 1, mpiMsgType, selectedMember, MSG_TAG, MPI_COMM_WORLD);
                printf("[myId: %d][clock: %d][to: %d]   Zapytanie o dolaczenie do grupy {%d}\n", memberId, localClock, selectedMember, myStatus);
            }

            //WAIT FOR STATUS UPDATE - RECIEVE RESPONSE MSG /
            while (myStatus == ALONE_STATUS || myStatus == MEMBER_STATUS || myStatus == LEADER_STATUS)
            {
                //WAIT FOR UPDATE FROM OTHER THREAD
            }

            //EXIT ASKING, ENOUGH MONEY TO ENTER CLUB
            exitWhile = false;

            switch (myStatus)
            {
            //TRANSITION STATE AFTER RECV ACCEPTED INVITE REQ
            case ACCEPT_INVITATION_STATUS:
                myStatus = LEADER_STATUS;
                if (groupMoney >= entryCost)
                {
                    printf("[myId: %d][clock: %d]           Zebraliśmy pieniądze: %d na %d. Wybieramy klub. {%d}\n", memberId, localClock, groupMoney, entryCost, myStatus);
                    exitWhile = true;
                }
                break;

            //TRANSITION STATE AFTER RECV REJECT INVITE REQ
            case REJECT_INVITATION_STATUS:
                myStatus = LEADER_STATUS;
                break;

            //WHEN LEADER SEND BREAK_MSG - NOT ENOUGH MONEY
            case GROUP_BREAK_STATUS:
                groupMoney = memberMoney;
                myStatus = ALONE_STATUS;
                exitWhile = true;
                initAgain = true;
                break;

            //WHEN MEMBER AND RECIEVE EXIT CLUB MSG
            case EXIT_CLUB_STATUS:
                printf("[myId: %d][clock: %d]           Wychodzę z klubuu [%d] {%d}\n", memberId, localClock, preferedClubId, myStatus);
                initAgain = true;
                exitWhile = true;
                break;
            }

            if (exitWhile)
            {
                break;
            }
        }
        if (!initAgain)
        {
            //WHEN HAS NO ENOUGH MONEY - GROUP BREAK
            if (groupMoney < entryCost && myStatus == LEADER_STATUS)
            {
                for (int i = 0; i < noMembers; i++)
                {
                    if (askTab[i] == ACCEPT_ASK_TAB && i != memberId)
                    {
                        localClock++;
                        sendMsg = createPackage(localClock, GROUP_BREAK_MSG, memberId, preferedClubId, memberMoney);
                        MPI_Send(&sendMsg, 1, mpiMsgType, i, MSG_TAG, MPI_COMM_WORLD);
                        printf("[myId: %d][clock: %d][to:   %d] Rozwiazanie grupy {%d}\n", memberId, localClock, i, myStatus);
                    }
                }
            }
            //WHEN HAS ENOUGH MONEY - ENTER TO CLUB
            else if (groupMoney >= entryCost && myStatus == LEADER_STATUS)
            {
                myStatus = ENOUGH_MONEY_STATUS;
                printf("[myId: %d][clock: %d] MAMY KLUB Wybralismy klub o nr: %d {%d}\n", memberId, localClock, preferedClubId, myStatus);
                //ASK EVERYONE - NOT IN MY GROUP
                for (int i = 0; i < noMembers; i++)
                {
                    if ((i != memberId) && (askTab[i] != ACCEPT_ASK_TAB))
                    {
                        localClock++;
                        sendMsg = createPackage(localClock, ASK_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
                        MPI_Send(&sendMsg, 1, mpiMsgType, i, MSG_TAG, MPI_COMM_WORLD);
                        printf("[myId: %d][clock: %d][to:   %d] Pytam sie o pozwolenie do klubu: %d {%d}\n", memberId, localClock, i, preferedClubId, myStatus);
                    }
                }
                printf("[myId: %d][clock: %d]           Wysłaliśmy wszytskim zapytanie o wejscie do klubu: %d {%d}\n", memberId, localClock, preferedClubId, myStatus);
                while (myStatus != ENTER_CLUB_STATUS)
                {
                    //WAIT FOR UPDATE FROM OTHER THREAD
                }

                printf("[myId: %d][clock: %d] WEJSCIE DO KLUBU Wchodzimy do klubu o nr: %d {%d}\n", memberId, localClock, preferedClubId, myStatus);

                //WHEN ENTER CLUB - SEND MSG TO MEMBERS THAT WE EXIT CLUB
                for (int i = 0; i < noMembers; i++)
                {
                    if (askTab[i] == ACCEPT_ASK_TAB && i != memberId)
                    {
                        localClock++;
                        sendMsg = createPackage(localClock, EXIT_CLUB_MSG, memberId, preferedClubId, memberMoney);
                        MPI_Send(&sendMsg, 1, mpiMsgType, i, MSG_TAG, MPI_COMM_WORLD);
                        printf("[myId: %d][clock: %d][to:   %d] Wychodzimy z klubu {%d}\n", memberId, localClock, i, myStatus);
                    }
                }
                //SEND MSG AFTER CLUBING THAT WE EXIT CLUB
                for (int i = 0; i < noMembers; i++)
                {
                    if (i != memberId && askTab[i] != ACCEPT_ASK_TAB)
                    {
                        localClock++;
                        sendMsg = createPackage(localClock, AGREE_TO_ENTER_CLUB_MSG, memberId, preferedClubId, memberMoney);
                        MPI_Send(&sendMsg, 1, mpiMsgType, i, MSG_TAG, MPI_COMM_WORLD);
                        printf("[myId: %d][clock: %d][to:   %d] Info o zwolnieniu klubu nr: %d {%d}\n", memberId, localClock, i, preferedClubId, myStatus);
                    }
                }

                printf("[myId: %d][clock: %d]        Kapitan wychodzi z klubu o nr: %d {%d}\n", memberId, localClock, preferedClubId, myStatus);
            }

            //EXIT CLUBS IN DIFFERERNT TIME
            //int sleepTime = rand() % (noMembers / 2);
            //localClock += sleepTime;
            //sleep(sleepTime);
            //printf("[myId: %d][clock: %d]           Spałem %d czasu\n", memberId, localClock, sleepTime);
        }
    }
}

int main(int argc, char *argv[])
{
    /* READ K & M FROM argv */
    noClubs = atoi(argv[1]);
    entryCost = atoi(argv[2]);
    //printf("K = %d M = %d\n", noClubs, entryCost);

    /* INIT MPI */
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &noMembers);
    MPI_Comm_rank(MPI_COMM_WORLD, &memberId);

    /* PREPARE STRUCT FOR SENDING MESSAGES */
    int blocklengths[MESSAGE_COUNT] = {1, 1, 1, 1, 1};
    MPI_Datatype types[MESSAGE_COUNT] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};

    MPI_Aint offsets[MESSAGE_COUNT];
    offsets[0] = offsetof(msg, localClock);
    offsets[1] = offsetof(msg, message);
    offsets[2] = offsetof(msg, memberId);
    offsets[3] = offsetof(msg, preferedClubId);
    offsets[4] = offsetof(msg, memberMoney);

    MPI_Type_create_struct(MESSAGE_COUNT, blocklengths, offsets, types, &mpiMsgType);
    MPI_Type_commit(&mpiMsgType);

    srand(time(0) + memberId);

    //INIT VALUE - WHEN RECIEVE MSG BEFORE MAINLOOP START
    if (memberMoney == 0)
    {
        threadMoneyFlag = true;
        memberMoney = (rand() % (entryCost - 2)) + 1;
        printf("[myId: %d][clock: %d]           my memberMoney = %d\n", memberId, localClock, memberMoney);
    }

    //RECIEVING THREAD
    pthread_t pthreadFunc;
    if (pthread_create(&pthreadFunc, NULL, threadFunc, NULL))
    {
        printf("[myId: %d]Error creating thread {%d}\n", memberId, myStatus);
        free(askTab);
        MPI_Type_free(&mpiMsgType);
        MPI_Finalize();
        exit(-1);
    }

    //MAIN SENDING THREAD
    mainLoop();

    printf("[myId: %d] koniec\n", memberId);
    /* MPI FINALIZE */
    MPI_Type_free(&mpiMsgType);
    MPI_Finalize();

    return 0;
}