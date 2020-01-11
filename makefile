all:
	make -C minimatrix/software/matrix-simpleclock
	make -C minimatrix/software/matrix-simpleclock/testcases
	make -C minimatrix/software/matrix-simpleclock/linux-gui
	make -C minimatrix/software/matrix-advancedclock
	make -C minimatrix/software/matrix-advancedclock/testcases
	make -C minimatrix/software/matrix-advancedclock/linux-gui

check:
	./minimatrix/software/matrix-simpleclock/testcases/runtestcases
	./minimatrix/software/matrix-advancedclock/testcases/runtestcases