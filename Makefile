# Compile options
CFLAGS = -lpthread -lrt
# Parameters
OBJS = OS_ergasia1.o
EXEC = OS_ergasia1

# Modify these
X = Stairway.txt 	# .txt file
K = 10				# Child processes
N = 10				# Transactions

# Arguments
ARGS = $(X)$(K)$(N)

# Compile
$(EXEC): $(OBJS)
	gcc $(OBJS) -o $(EXEC) $(CFLAGS)

run: $(EXEC)
	./$(EXEC) $(ARGS)

clean:
	rm -f $(OBJS) $(EXEC)