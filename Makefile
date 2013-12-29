all :
	gcc -c BBBiolib.c
	ar -rs libBBBio.a BBBiolib.o
	gcc -c dds.c -o dds.o
	gcc dds.o -L. -lBBBio -lm -o dds
clean :
	rm -rf *.o libBBBio.a dds.o dds
