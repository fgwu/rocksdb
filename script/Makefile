all:
	find . -type f -name "*.c" -o -type f -name "*.cc" -o -type f -name "*.h" > cscope.files
	cscope -R -b -i cscope.files
