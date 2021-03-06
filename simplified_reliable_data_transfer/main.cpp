#include <cstdio>
#include <cstdlib>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional or bidirectional
   data transfer protocols (from A to B. Bidirectional transfer of data
   is for extra credit and is not required).  Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

#define BIDIRECTIONAL 0    /* change to 1 if you're doing extra credit */
/* and write a routine called B_output */

/* a "msg" is the data unit passed from layer 5 (teachers code) to layer  */
/* 4 (students' code).  It contains the data (characters) to be delivered */
/* to layer 5 via the students transport level protocol entities.         */
struct msg {
  char data[20];
};

/* a packet is the data unit passed from layer 4 (students code) to layer */
/* 3 (teachers code).  Note the pre-defined packet structure, which all   */
/* students must follow. */
struct pkt {
  int seqnum;
  int acknum;
  int checksum;
  char payload[20];
};

/*****************************************************************
***************** NETWORK EMULATION CODE STARTS BELOW ***********
The code below emulates the layer 3 and below network environment:
  - emulates the tranmission and delivery (possibly with bit-level corruption
    and packet loss) of packets across the layer 3/4 interface
  - handles the starting/stopping of a timer, and generates timer
    interrupts (resulting in calling students timer handler).
  - generates message to be sent (passed from later 5 to 4)

THERE IS NOT REASON THAT ANY STUDENT SHOULD HAVE TO READ OR UNDERSTAND
THE CODE BELOW.  YOU SHOLD NOT TOUCH, OR REFERENCE (in your code) ANY
OF THE DATA STRUCTURES BELOW.  If you're interested in how I designed
the emulator, you're welcome to look at the code - but again, you should have
to, and you defeinitely should not have to modify
******************************************************************/

struct event {
  float evtime;           /* event time */
  int evtype;             /* event type code */
  int eventity;           /* entity where event occurs */
  struct pkt *pktptr;     /* ptr to packet (if any) assoc w/ this event */
  struct event *prev;
  struct event *next;
};
struct event *evlist = nullptr;   /* the event list */

/* possible events: */
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

#define  OFF             0
#define  ON              1
#define   A    0
#define   B    1



int TRACE = 1;             /* for my debugging */
int nsim = 0;              /* number of messages from 5 to 4 so far */
int nsimmax = 0;           /* number of msgs to generate, then stop */
float time = 0.000;
float lossprob;            /* probability that a packet is dropped  */
float corruptprob;         /* probability that one bit is packet is flipped */
float lambda;              /* arrival rate of messages from layer 5 */
int   ntolayer3;           /* number sent into layer 3 */
int   nlost;               /* number lost in media */
int ncorrupt;              /* number corrupted by media*/





/****************************************************************************/
/* jimsrand(): return a float in range [0,1].  The routine below is used to */
/* isolate all random number generation in one location.  We assume that the*/
/* system-supplied rand() function return an int in therange [0,mmm]        */
/****************************************************************************/
float jimsrand()
{
  auto x = (float)(rand()/double(RAND_MAX));
  return(x);
}

/********************* EVENT HANDLINE ROUTINES *******/
/*  The next set of routines handle the event list   */
/*****************************************************/
void insertevent(event *p) {
  event *q, *qold;

  if (TRACE > 2) {
    printf("            INSERTEVENT: time is %lf\n", time);
    printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
  }
  q = evlist;     /* q points to header of list in which p struct inserted */
  if (q == nullptr) {   /* list is empty */
    evlist = p;
    p->next = nullptr;
    p->prev = nullptr;
  } else {
    for (qold = q; q != nullptr && p->evtime > q->evtime; q = q->next)
      qold = q;
    if (q == nullptr) {   /* end of list */
      qold->next = p;
      p->prev = qold;
      p->next = nullptr;
    } else if (q == evlist) { /* front of list */
      p->next = evlist;
      p->prev = nullptr;
      p->next->prev = p;
      evlist = p;
    } else {     /* middle of list */
      p->next = q;
      p->prev = q->prev;
      q->prev->next = p;
      q->prev = p;
    }
  }
}

void generate_next_arrival() {
  double log(), ceil();
  float ttime;
  int tempint;

  if (TRACE > 2)
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");

  double x = lambda * jimsrand() * 2;  /* x is uniform on [0,2*lambda] */
  /* having mean of lambda        */

  auto *evptr = new event;
  evptr->evtime = (float)(time + x);
  evptr->evtype = FROM_LAYER5;
  if (BIDIRECTIONAL && (jimsrand() > 0.5))
    evptr->eventity = B;
  else
    evptr->eventity = A;
  insertevent(evptr);
}

void printevlist()
{
  event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=nullptr; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n",q->evtime,q->evtype,q->eventity);
  }
  printf("--------------\n");
}



/********************** Student-callable ROUTINES ***********************/

/* called by students routine to cancel a previously-started timer */
void stoptimer(int AorB) /* A or B is trying to stop timer */
{
  event *q, *qold;

  if (TRACE > 2)
    printf("          STOP TIMER: stopping timer at %f\n", time);

  /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
  for (q = evlist; q != nullptr; q = q->next)
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      /* remove this event */
      if (q->next == nullptr && q->prev == nullptr)
        evlist = nullptr;         /* remove first and only event on list */
      else if (q->next == nullptr) /* end of list - there is one in front */
        q->prev->next = nullptr;
      else if (q == evlist) { /* front of list - there must be event after */
        q->next->prev = nullptr;
        evlist = q->next;
      } else {     /* middle of list */
        q->next->prev = q->prev;
        q->prev->next = q->next;
      }

      free(q);
      return;
    }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}


void starttimer(int AorB, float increment) /* A or B is trying to stop timer */
{
  event *q;

  if (TRACE > 2)
    printf("          START TIMER: starting timer at %f\n", time);

  /* be nice: check to see if timer is already started, if so, then  warn */
  /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next)  */
  for (q = evlist; q != nullptr; q = q->next)
    if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }

  /* create future event for when timer goes off */
  auto *evptr = new event;
  evptr->evtime = time + increment;
  evptr->evtype = TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}


void tolayer3(int AorB, pkt packet) /* A or B is trying to stop timer */
{
  event *q;
  ntolayer3++;

/* simulate losses: */
  if (jimsrand() < lossprob) {
    nlost++;
    if (TRACE > 0)
      printf("          TOLAYER3: packet being lost\n");
    return;
  }

  /* make a copy of the packet student just gave me since he/she may decide */
  /* to do something with the packet after we return back to him/her */
  auto mypktptr = new pkt;
  mypktptr->seqnum = packet.seqnum;
  mypktptr->acknum = packet.acknum;
  mypktptr->checksum = packet.checksum;
  for (int i = 0; i < 20; i++)
    mypktptr->payload[i] = packet.payload[i];
  if (TRACE > 2) {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
           mypktptr->acknum, mypktptr->checksum);
    for (int i = 0; i < 20; i++)
      printf("%c", mypktptr->payload[i]);
    printf("\n");
  }

  /* create future event for arrival of packet at the other side */

  //evptr = (struct event *) malloc(sizeof(struct event));
  auto *evptr = new event;

  evptr->evtype = FROM_LAYER3;   /* packet will pop out from layer3 */
  evptr->eventity = (AorB + 1) % 2; /* event occurs at other entity */
  evptr->pktptr = mypktptr;       /* save ptr to my copy of packet */
  /* finally, compute the arrival time of packet at the other end.
   medium can not reorder, so make sure packet arrives between 1 and 10
   time units after the latest arrival time of packets
   currently in the medium on their way to the destination */
  float lastime = time;
  /* for (q=evlist; q!=NULL && q->next!=NULL; q = q->next) */
  for (q = evlist; q != nullptr; q = q->next){
    if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity))
      lastime = q->evtime;
  }
  evptr->evtime = lastime + 1 + 9 * jimsrand();


  /* simulate corruption: */
  if (jimsrand() < corruptprob) {
    ncorrupt++;
    float x = jimsrand();
    if (x < .75)
      mypktptr->payload[0] = 'Z';   /* corrupt payload */
    else if (x < .875)
      mypktptr->seqnum = 999999;
    else
      mypktptr->acknum = 999999;
    if (TRACE > 0)
      printf("          TOLAYER3: packet being corrupted\n");
  }

  if (TRACE > 2)
    printf("          TOLAYER3: scheduling arrival on other side\n");
  insertevent(evptr);
}

// Assume datasent contains only 20 char
// TODO: security issue?
void tolayer5(int AorB, char datasent[]) {
  if (TRACE > 2) {
    printf("          TOLAYER5: data received: ");
    for (int i = 0; i < 20; i++)
      printf("%c", datasent[i]);
    printf("\n");
  }
}

/************************************************************************/



/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/
/*Control parameters*/
int maxSeqNum = 2;
float timerWaitDuration = 5000; // number of time unit
int ModuloAddition(int oldNum, int inc)
{
  return (oldNum + inc) % maxSeqNum;
}

int checksum(pkt *packet)
{
  int sum = 0;
  sum += packet->seqnum;
  sum += packet->acknum;
  // ignore the checksum field
  for (int i = 0; i < 20; i++)
    sum += packet->payload[i];

  // payload is 20 bytes, max is 20 * 255 = 5100
  // seqnum and acknum is 0 or 1
  // max of sum is 5100
  // i.e. will not overflow
  return sum;
}

pkt makePacket(int seqNum, int ackNum, char data[])
{
  pkt packet{};
  packet.seqnum = seqNum;
  packet.acknum = ackNum;
  for (int i = 0; i < 20; i++)
    packet.payload[i] = data[i];
  packet.checksum = checksum(&packet);

  return packet;
}


/*State parameters */
bool A_waitingState;
pkt A_lastSendPacket;
pkt A_lastSReceivedPacket;

/* called from layer 5, passed the data to be sent to other side */
void A_output(msg message)
{
  if(A_waitingState){
    // Drop the message if we are still waiting for acknowledgement
    return;
  }

  // For alternating-bit-protocol, the sequent number is 0 or 1.
  // In a more realistic situation, seqNumInc = size(packet.payload)
  int seqNumInc = 1;
  int seqNum = ModuloAddition(A_lastSendPacket.seqnum, seqNumInc);
  int ackNum = A_lastSReceivedPacket.seqnum;
  pkt packet = makePacket(seqNum, ackNum, message.data);

  tolayer3(A, packet);

  A_waitingState = true;
  A_lastSendPacket = packet;

  starttimer(A, timerWaitDuration);
}

void B_output(msg message)  /* need be completed only for extra credit */
{

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(pkt packet)
{
  if(packet.acknum != A_lastSendPacket.seqnum){
    // the arrived packet is not the expected
    return;
  }
  if(packet.checksum != checksum(&packet)){
    // the packet is corrupted
    return;
  }

  // No need to pass data to layer5

  A_waitingState = false;
  stoptimer(A);

  A_lastSReceivedPacket = packet;
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  // Resend packet
  tolayer3(A, A_lastSendPacket);

  starttimer(A, timerWaitDuration);
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  A_lastSendPacket = pkt();
  A_lastSendPacket.seqnum = 1;
  A_lastSendPacket.acknum = 0;

  A_waitingState = false;
}


/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
pkt B_LastSentPacket;
pkt B_LastReceivedPacket;
void B_input(pkt packet)
{
  int B_expectedSeqNum = ModuloAddition(B_LastSentPacket.acknum, 1);
  if(packet.seqnum != B_expectedSeqNum){
    // the arrived packet is not the expected
    tolayer3(B, B_LastSentPacket);
    return;
  }
  if(packet.checksum != checksum(&packet)){
    // the packet is corrupted
    tolayer3(B, B_LastSentPacket);
    return;
  }

  // expected packet arrived, pass the payload to application layer
  tolayer5(B, packet.payload);

  // Acknowledge correctly received packet
  // For alternating-bit-protocol, the sequent number is 0 or 1.
  // In a more realistic situation, ackNumInc = size(packet.payload)
  int ackNumInc = 1;
  int ackNum = (B_LastReceivedPacket.seqnum + ackNumInc) % maxSeqNum;
  int seqNum = (B_LastSentPacket.seqnum + 1) % maxSeqNum;

  // Since there is no field for declaring payload size, we
  // create empty payload for the sake of simple coding
  char data[20];
  for (int i =0; i<20; i++)
    data[i] = 0;
  pkt ackPkt = makePacket(seqNum, ackNum, data);

  tolayer3(B, ackPkt);

  B_LastSentPacket = ackPkt;
  B_LastReceivedPacket = packet;
}

/* called when B's timer goes off */
void B_timerinterrupt()
{
}

/* the following rouytine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  B_LastSentPacket = pkt();
  B_LastSentPacket.seqnum = 1;
  B_LastSentPacket.acknum = 1;

  B_LastReceivedPacket = pkt();
  B_LastReceivedPacket.seqnum = 1;
  B_LastReceivedPacket.acknum = 1;
}

/*
 * Note that, because there is no handshaking process, we can't use random
 * number for the initial sequence number.
 */

/************************************************************************/


/* initialize the simulator */
void init()
{
  nsimmax = 10;
  lossprob = .0;
  corruptprob = .0;
  lambda = 1000;
  TRACE = 10;

  /*
  printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
  printf("Enter the number of messages to simulate: ");
  scanf("%d", &nsimmax);
  printf("Enter  packet loss probability [enter 0.0 for no loss]:");
  scanf("%f", &lossprob);
  printf("Enter packet corruption probability [0.0 for no corruption]:");
  scanf("%f", &corruptprob);
  printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
  scanf("%f", &lambda);
  printf("Enter TRACE:");
  scanf("%d", &TRACE);
  */


  srand(9999);              /* init random number generator */
  float sum = 0.0;                /* test random number generator for students */
  for (int i = 0; i < 1000; i++)
    sum = sum + jimsrand();    /* jimsrand() should be uniform in [0,1] */
  float avg = sum / 1000.0f;
  if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n");
    printf("is different from what this emulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
    exit(-1);
  }

  ntolayer3 = 0;
  nlost = 0;
  ncorrupt = 0;

  time = 0.0;                    /* initialize time to 0.0 */
  generate_next_arrival();     /* initialize event list */
}

int main()
{
  event *eventptr;
  msg  msg2give{};
  pkt  pkt2give{};

  char c;

  init();
  A_init();
  B_init();

  while (true) {
    /* get next event to simulate */
    eventptr = evlist;
    if (eventptr == nullptr) {
      printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
      exit(0);
    }

    /* remove this event from event list */
    evlist = evlist->next;
    if (evlist != nullptr)
      evlist->prev = nullptr;

    if (TRACE >= 2) {
      printf("\nEVENT time: %f,", eventptr->evtime);
      printf("  type: %d", eventptr->evtype);
      if (eventptr->evtype == 0)
        printf(", timerinterrupt  ");
      else if (eventptr->evtype == 1)
        printf(", fromlayer5 ");
      else
        printf(", fromlayer3 ");
      printf(" entity: %d\n", eventptr->eventity);
    }

    /* update time to next event time */
    time = eventptr->evtime;
    if (nsim == nsimmax) {
      /* all done with simulation */
      break;
    }

    /* Handle events */
    if (eventptr->evtype == FROM_LAYER5) { // send msg
      /* set up future arrival */
      generate_next_arrival();

      /* fill in msg to give with string of same letter */
      int j = nsim % 26;
      for (int i = 0; i < 20; i++)
        msg2give.data[i] = 97 + j;
      if (TRACE > 2) {
        printf("          MAINLOOP: data given to student: ");
        for (int i = 0; i < 20; i++)
          printf("%c", msg2give.data[i]);
        printf("\n");
      }

      nsim++;
      if (eventptr->eventity == A)
        A_output(msg2give);
      else
        B_output(msg2give);

    } else if (eventptr->evtype == FROM_LAYER3) { // msg arrived
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      for (int i = 0; i < 20; i++)
        pkt2give.payload[i] = eventptr->pktptr->payload[i];

      /* deliver packet by calling */
      if (eventptr->eventity == A)
        A_input(pkt2give);            /* appropriate entity */
      else
        B_input(pkt2give);
      free(eventptr->pktptr);

    } else if (eventptr->evtype == TIMER_INTERRUPT) { // timer times up
      if (eventptr->eventity == A)
        A_timerinterrupt();
      else
        B_timerinterrupt();
    } else {
      printf("INTERNAL PANIC: unknown event type \n");
    }
    free(eventptr);
  }

  return 0;
}
