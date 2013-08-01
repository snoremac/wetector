export WETECTOR_HOME = .
export WETECTOR_SRC = $(WETECTOR_HOME)/src/wetector
export SENSIMATIC_HOME = $(WETECTOR_HOME)/../sensimatic
export SENSIMATIC_SRC = $(SENSIMATIC_HOME)/src/sensimatic

include $(SENSIMATIC_HOME)/mk/defs.mk
include $(SENSIMATIC_HOME)/mk/common.mk

DEFAULT_LDFLAGS += -lprintf_flt -Wl,-u,vfprintf

.PHONY: all debug install
all : wetector
debug : wetector-debug
install : wetector-install

INCLUDES = -I. -I$(WETECTOR_SRC) -I$(SENSIMATIC_SRC) -I$(SENSIMATIC_SRC)/hal

.PHONY: wetector
wetector : $(WETECTOR_HOME)/wetector.hex

wetector_src = $(wildcard $(WETECTOR_SRC)/*.c)
wetector_obj = $(wetector_src:.c=.o)

$(WETECTOR_HOME)/wetector.elf : $(wetector_obj) $(SENSIMATIC_SRC)/sensimatic.a
	$(CC) $(DEFAULT_LDFLAGS) $(LDFLAGS) -o $@ $^

.PHONY: $(SENSIMATIC_SRC)/sensimatic.a
$(SENSIMATIC_SRC)/sensimatic.a : 
	make -C $(SENSIMATIC_HOME) sensimatic INCLUDES=-I$(realpath $(WETECTOR_SRC)) LOG_LEVEL=$(LOG_LEVEL)

.PHONY: clean
clean:
	rm -f $(addprefix $(WETECTOR_HOME)/, $(ARTIFACTS))
	rm -f $(addprefix $(WETECTOR_SRC)/, $(ARTIFACTS))
	rm -f $(addprefix $(SENSIMATIC_SRC)/, $(ARTIFACTS))
	make -C $(SENSIMATIC_HOME) clean
