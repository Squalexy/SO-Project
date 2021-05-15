CC		= gcc
FLAGS	= -pthread -D_REENTRANT -Wall -g -O0
OBJS	= main.o functions.o processes.o
PROG	= races

all:		${PROG}

clean:
			rm ${OBJS} *~ ${PROG}

${PROG}:	${OBJS}
			${CC} ${FLAGS} ${OBJS} -o $@
		
.c.o:
			${CC} ${FLAGS} $< -c -o $@


functions.o: functions.c declarations.h

processes.o: processes.c declarations.h

main.o: main.c declarations.h 

races: main.o functions.o processes.o
