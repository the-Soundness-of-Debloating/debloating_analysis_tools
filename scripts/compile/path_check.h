/*

add #include "path_check.h" at the beginning of prog.c
add path_check(argc,argv); at the beginning of main()

*/
int path_check(int argc,char *argv[])
{
        int loop_var = 1;
        while(loop_var < argc)
        {
		    for(int j=0;j<1;j++){
                char *t = argv[loop_var];
		        if(*t && *t=='/'){
                   printf("arg contains invalid content '/'\n");
                   exit(1);
                }
                else if(*t && *t =='.'&& (*t++)== '.'){
                   printf("arg contains invalid content '..'\n");
                   exit(1);
                }
                else if(*t && *t =='.'&& (*t++)== '/'){
                   printf("arg contains invalid content './'\n");
                   exit(1);
                }
                
		    }
		    loop_var++;
        }
        return 0;
}



