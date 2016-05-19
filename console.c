/* 
   ==========================================================================
   Console.c
   Author: Joseph Emmanuel Dayo
   Date updated:December 6, 2002
   Description: A kernel mode console that is used for debugging the kernel
   and testing new kernel features.
                
    DEX educational extensible operating system 1.0 Beta
    Copyright (C) 2004  Joseph Emmanuel DL Dayo

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. 
   ==========================================================================
*/

#include "console.h"
  
/*A console mode get string function terminates
upon receving \r */
void getstring(char *buf,DEX32_DDL_INFO *dev)
  {
    unsigned int i=0;
    char c;
    do
    {
    c=getch();
    if (c=='\r'||c=='\n'||c==0xa) break;
    if (c=='\b' || (unsigned char)c == 145)
       {
       if(i>0)
        {
        i--;

        if (Dex32GetX(dev)==0)
             {
              Dex32SetX(dev,79);
              if (Dex32GetY(dev)>0) Dex32SetY(dev,Dex32GetY(dev)-1);
             }
             else
        Dex32SetX(dev,Dex32GetX(dev)-1);     
        Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),' ',Dex32GetAttb(dev));
        };
       }
       else
       {

        if (i<256)  //maximum command line is only 255 characters
         {
            Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),buf[i]=c,Dex32GetAttb(dev));
            i++;
            Dex32SetX(dev,Dex32GetX(dev)+1);     
            if (Dex32GetX(dev)>79) {Dex32SetX(dev,0);
            Dex32NextLn(dev);};
         };
       };

     Dex32PutChar(dev,Dex32GetX(dev),Dex32GetY(dev),' ',Dex32GetAttb(dev));
     update_cursor(Dex32GetY(dev),Dex32GetX(dev));
    } while (c!='\r');
    
    Dex32SetX(dev,0);
    Dex32NextLn(dev);
    buf[i]=0;
  };

/*Show information about memory usage. This function is also useful
  for detecting memory leaks*/
void meminfo()
  {
    DWORD totalbytes = totalpages * 0x1000;
    DWORD freebytes = totalbytes - (totalpages - stackbase[0])* 0x1000;
    printf("=================Memory Information===============\n");
    printf("Pages available     : %10u pages\n",stackbase[0]);
    printf("Total Pages         : %10d pages\n",totalpages);
    printf("Total Memory        : %10u bytes (%10d KB)\n",totalbytes, totalbytes / 1024);
    printf("Free Memory         : %10u bytes (%10d KB)\n",freebytes, freebytes / 1024);
    printf("used pages          : %10d pages (%10d KB)\n",totalpages-stackbase[0],
    (totalpages-stackbase[0])*0x1000);
  };

int delfile(char *fname)
  {
    int sectors;
    file_PCB *f=openfilex(fname,FILE_WRITE);
    return fdelete(f);
  };

int user_fork()
{
    int curval = current_process->processid;
    int childready = 0, retval = 0;
    int hdl;
    int id;
    DWORD flags;
    #ifdef DEBUG_FORK
    printf("user_fork called\n");
    #endif
    
    //enable interrupts since we want the process dispatcher to take control
    storeflags(&flags);
    startints();
   
    hdl = pd_forkmodule(current_process->processid);
    
    taskswitch();  
    id = pd_dispatched(hdl);
    //while (!(id=pd_dispatched(hdl)))
    //  ;
    
    if (curval != current_process->processid) //this is the child
      {
        //If this is the child process, the processid when this function
        //was called is not equal to the current processid.
        retval = 0;
      };
      
    if (curval == current_process->processid) // this is the parent
      {
        pd_ok(hdl);
        retval = id;
      };
      
      restoreflags(flags);
      return retval;
};

int user_execp(char *fname,DWORD mode,char *params)
 {
  DWORD id,size;
  char *buf;
  vfs_stat filestat;
  file_PCB *f=openfilex(fname,0);
  
  if (f!=0)
  {
      fstat(f,&filestat);
      size = filestat.st_size;
      buf=(char*)malloc(filestat.st_size+511);
      //tell the vfs to increase buffersize to speed up reads
      vfs_setbuffer(f,0,filestat.st_size,FILE_IOFBF);
      if (fread(buf,size,1,f)==size)
      {
        char temp[255];
        #ifdef DEBUG_USER_PROCESS
        printf("execp(): adding module..\n");
        #endif
        int hdl= addmodule(fname,buf,userspace,mode,params,showpath(temp),getprocessid());
        
        #ifdef DEBUG_USER_PROCESS
        printf("execp(): done.\n");
        #endif

        taskswitch();
        #ifdef DEBUG-USER_PROCESS
        printf("execp(): parent waiting for child to finish\n");
        #endif
        
        while (!(id=pd_ok(hdl))) ; //process already running?
        fg_setmykeyboard(id);
        dex32_waitpid(id,0);
        //dex32_wait();
        fg_setmykeyboard(getprocessid());
      };
     free(buf);
     fclose(f);
     return id;
  };
  return 0;
 ;};

int exec(char *fname,DWORD mode,char *params)
 {
  DWORD id;
  char *buf;
  file_PCB *f=openfilex(fname,0);

  if (f!=0)
  {
     DWORD size;
     char temp[255];
     vfs_stat fileinfo;
     fstat(f,&fileinfo);
     size = fileinfo.st_size;
     
     buf=(char*)malloc(size+511);
     
     if (fread(buf,size,1,f)==size)
     id=dex32_loader(fname,buf,userspace,mode,params,showpath(temp),getprocessid());


     free(buf);
     fclose(f);
     return id;
  };
  return 0;
 ;};


int user_exec(char *fname,DWORD mode,char *params)
 {
  DWORD id;
  char *buf;
  file_PCB *f=openfilex(fname,FILE_READ);

  if (f!=0)
  {
     int hdl,size;
     char temp[255];
     vfs_stat fileinfo;
     fstat(f,&fileinfo);
     size = fileinfo.st_size;
     buf=(char*)malloc(size+511);
     fread(buf,size,1,f);
     hdl = addmodule(fname,buf,userspace,mode,params,showpath(temp),getprocessid());
     while (!pd_ok(hdl)) ;
     
     free(buf);
     fclose(f);
     return id;
  };
  return 0;
 ;};

int loadDLL(char *name,char *p)
{
 file_PCB *handle;
 int fsize; vfs_stat filestat;
 int hdl,libid;
 char *buf;

 handle=openfilex(name,FILE_READ);
 if (!handle) return -1;
 fstat(handle,&filestat);
 vfs_setbuffer(handle,0,filestat.st_size,FILE_IOFBF);
 //get filesize and allocate memory
 fsize= filestat.st_size;
 buf=(char*)malloc(fsize);
 
 //load the file from the disk 
 fread(buf,fsize,1,handle);
 
 /*Tell the process dispatcher to map the file into memory and
   create data structures necessary for managing dynamic libraries*/
   
 hdl = addmodule(name,buf,lmodeproc,PE_USERDLL,p,0,getprocessid());
 
 //wait until the library has been loaded before we continue since addmodule returns immediately
 while (!(libid = pd_ok(hdl)));
 
 //done!
 free(buf);

 //close the file
 fclose(handle);
 return libid;
};

void loadfile(char *s,int,int);

void loadlib(char *s)
  {
 char *buf;
   DWORD size;
 loadDLL(s,0);
 };


int console_showfile(char *s,int wait)
 {
   char *buf;
   DWORD size;
   file_PCB *handle;
   vfs_stat fileinfo;
   int i;
   DEX32_DDL_INFO *myddl;
   handle=openfilex(s,FILE_READ);
   if (!handle) return -1;
   fstat(handle,&fileinfo);
   size = fileinfo.st_size;
   buf=(char*)malloc(size);
   textbackground(BLUE);
   myddl = Dex32GetProcessDevice();
   printf("Name: %s  size of file: %d\n",s,size);
   textbackground(BLACK);
   fread(buf,size,1,handle);
    for (i=0;i<size;i++)
       {
           if (buf[i]!='\r') 
           printf("%c",buf[i]);

           if (myddl->lines%25==0)  
              { 
                 char c;
                 printf("\nPress any key to continue, 'q' to quit\n");
                 c=getch();
                 if (c=='q') break;
             };
       };
    fclose(handle);
    free(buf);
    return 1;
};
//creates a virtual console for a process
DWORD alloc_console()
{
dex32_commit(0xB8000,1,current_process->pagedirloc,PG_WR)
;};


void console()
 {
   
    console_main();
 };


void prompt_parser(const char *promptstr,char *prompt)
  {
  int i,i2=0,i3=0;
  char command[10],temp[255];
  strcpy(prompt,"");
  for (i=0;promptstr[i]&&i<255;i++)
    {
      if (promptstr[i]!='%')
          //add to the prompt
          {
              prompt[i2]=promptstr[i];
              i2++;
              prompt[i2]=0;
          }
          else
          {
            if (promptstr[i+1]!=0)
              {
                if (promptstr[i+1]=='%')
                   {
                   prompt[i2]='%';
                   i2++;
                   prompt[i2]=0;
                   i+=2;
                   continue;
                   };
              };
            i3=0;
            for (i2=i+1;promptstr[i2]&&i2<255;i2++)
              {

                  if (promptstr[i2]=='%'||i3>=10) break;
                  command[i3]=promptstr[i2];
                  i3++;

              };
              i=i2;
              command[i3]=0;
              if (strcmp(command,"cdir")==0)
                {
                  strcat(prompt,showpath(temp));
                  i2=strlen(prompt);
                };
          };

    };

  };
  


/* ==================================================================
   console_ls(int style):
   
   *list the contents of the current directory to the screen
    style = 1      : list format 
    sty;e = others : wide format  

*/

/*An auxillary function for qsort for comparing two elements*/
int console_ls_sortsize(vfs_node *n1,vfs_node *n2)
{
    if (n1->size > n2->size) return -1;
    if (n2->size > n1->size) return 1;
    return 0;
};

/*An auxillary function for qsort for comparing two elements*/
int console_ls_sortname(vfs_node *n1,vfs_node *n2)
{
    if ( (n1->attb & FILE_DIRECTORY) && !(n2->attb & FILE_DIRECTORY))
        return -1;

    if ( !(n1->attb & FILE_DIRECTORY) && (n2->attb & FILE_DIRECTORY))
        return 1;
        
    return strsort(n1->name,n2->name);
};

/*lists the files in the current directory to the console screen*/
void console_ls(int style, int sortmethod)
{

    vfs_node *dptr=current_process->workdir;
    vfs_node *buffer;
    int totalbytes=0,freebytes=0;
    int totalfiles=0,i;
    char cdatestr[20],mdatestr[20],temp[20];
    
    //obtain total number of files
    totalfiles = vfs_listdir(dptr,0,0);
    
    buffer = (vfs_node*) malloc( totalfiles * sizeof(vfs_node));

    //Place the list of files obtained from the VFS into a buffer
    totalfiles = vfs_listdir(dptr,buffer,totalfiles* sizeof(vfs_node));     
    
    //Sort the list
    if (sortmethod == SORT_NAME)
        qsort(buffer,totalfiles,sizeof(vfs_node),console_ls_sortname);
    else
    if (sortmethod == SORT_SIZE)
        qsort(buffer,totalfiles,sizeof(vfs_node),console_ls_sortsize);
        
    textbackground(BLUE);
    textcolor(WHITE);
    
    if (style==1)
        printf("%-25s %10s %14s %14s\n","Filename","Size(bytes)","Attribute","Date Modified");
        
    textbackground(BLACK);

    for (i=0; i < totalfiles; i++)
    {
        char fname[255];   
        if (style==0) //wide view style
        {
            
            if (buffer[i].attb&FILE_MOUNT)
                textcolor(LIGHTBLUE);
            else
                if (buffer[i].attb&FILE_DIRECTORY)
                    textcolor(GREEN);
                else
                    if (buffer[i].attb&FILE_OEXE)
                        textcolor(YELLOW);
                    else
                    textcolor(WHITE);

            strcpy(fname,buffer[i].name);
            fname[24]=0;
            printf("%-25s ",fname);
            totalbytes+=buffer[i].size;
            
            if ( (i+1)%3==0 && (i+1 < totalfiles) ) printf("\n");

        };

        if (style==1) //list view style
        {
            if (buffer[i].attb&FILE_MOUNT)
                textcolor(LIGHTBLUE);
            else
                if (buffer[i].attb&FILE_DIRECTORY)
                    textcolor(GREEN);
                else
                     if (buffer[i].attb&FILE_OEXE)
                        textcolor(YELLOW);
                    else
                    textcolor(WHITE);
                    
            strcpy(fname,buffer[i].name);
            fname[24]=0;
            printf("%-25s ",fname);
            
            textcolor(WHITE);
            printf("%10d %14s %14s\n",buffer[i].size,
            vfs_attbstr(&buffer[i],temp),datetostr(&buffer[i].date_modified,
                       mdatestr));
                       
            totalbytes+=buffer[i].size;


            //try to make it fit the screen
            if ((i+1) % 23==0) 
            {
                char c;
                printf("Press Q to quit or any other key to continue ...");
                c=getch();
                printf("\n");
                if (c=='q'||c=='Q') break;
            };
        };       
    };
    
    textcolor(WHITE);
    printf("\nTotal Files: %d  Total Size: %d bytes\n", totalfiles, totalbytes);
    free(buffer);
    
};

/*
	CMSC 125 Project =================================================================
*/
int determine_first_day(int year){
	int daycode;
	int a, b, c;
	
	a = (year - 1.)/ 4.0;
	b = (year - 1.)/ 100.;
	c = (year - 1.)/ 400.;
	daycode = (year + a - b + c) %7;
	return daycode;
}

void printSpace(int lenOfString){
	int i, spaces = 22 - lenOfString;
	for (i = 0; i < spaces; ++i)
		printf(" ");
}
void printGap(int day, int daycode, int lastday){
	int j, space;
	
	if(day == 143){
		printf("                      ");
	}
	else if((day-1) == lastday){
		// printf("[%d]", (day-2+daycode)%7);
		switch(((day-2)+daycode)%7){
			case 0:	space = 18; break;
			case 1:	space = 15; break;
			case 2:	space = 12; break;
			case 3:	space = 9; break;
			case 4:	space = 6; break;
			case 5:	space = 3; break;
			case 6:	space = 0; break;
		}
		for(j=0; j<space; j+=1)
			printf(" ");
		printf(" ");
	}
	else{
		printf(" ");
	}
}
void printDays(int * day, int daycode, int m, int y){
	int d = *day;
	if (d <= m) {
		do
		{	

			if(time_systime.day == d && time_systime.month == m && time_systime.year == y)
				textcolor(GREEN);
			printf("%2d ", d);
			textcolor(WHITE);
			d += 1;
		} while (((d + daycode) % 7) != 1 && d <= m);
		*day = d;
	}
}
void show_year_calendar(int year){
	// stuff needed for making the calendar
	int days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
	char *months[] = {
		" ",
		"January",
		"February",
		"March",
		"April",
		"May",
		"June",
		"July",
		"August",
		"September",
		"October",
		"November",
		"December"
	};

	int month, month2, month3, day, day2, day3, i, daycode2, daycode3, j, space;
	// determine the day of the January 1 year
	int daycode = determine_first_day(year);
	
	// check if leap year
	if(year% 4 == 0 && year%100 != 0 || year%400 == 0)
		days_in_month[2] = 29;
	else
		days_in_month[2] = 28;


	// start printing calendar
	printf("\n                              %d", year);
	
	for ( month = 1; month <= 12; month+=3 )
	{
		// initialize the next two months with respect to 'month' var
		month2 = month+1; month3 = month+2;

		textcolor(YELLOW);
		// print months
		printf("\n%s", months[month]);
		printSpace(strlen(months[month]));
		printf("%s", months[month2]);
		printSpace(strlen(months[month2]));
		printf("%s", months[month3]);
		textcolor(WHITE);

		// print days in a week
		printf("\nSu Mo Tu We Th Fr Sa  Su Mo Tu We Th Fr Sa  Su Mo Tu We Th Fr Sa\n" );
		
		// initialize the starting dates of each month
		day = 1, day2 = 1, day3 = 1;

		// initialize the starting day of the next two months
		daycode2 = ( daycode + days_in_month[month] ) % 7;
		daycode3 = ( daycode2 + days_in_month[month2] ) % 7;

		// loop printing the week by week of 'month' -> 'month' + 2
		while(day <= days_in_month[month] || day2 <= days_in_month[month2] || day3 <= days_in_month[month3]){
			// print first month ----------------------------------
			if(day == 1){
				for(i=0; i<daycode; i+=1)
					printf("   ");
			}
			printDays(&day, daycode, days_in_month[month], year);
			printGap(day, daycode, days_in_month[month]);
			if(day-1 == days_in_month[month])
				day = 143;

			// print second month ----------------------------------
			if(day2 == 1){
				for(i=0; i<daycode2; i+=1)
					printf("   ");
			}
			printDays(&day2, daycode2, days_in_month[month2], year);
			printGap(day2, daycode2, days_in_month[month2]);
			if(day2-1 == days_in_month[month2])
				day2 = 143;

			// print third month ----------------------------------
			if(day3 == 1){
				for(i=0; i<daycode3; i+=1)
					printf("   ");
			}
			printDays(&day3, daycode3, days_in_month[month3], year);
			printf("\n");
		}
		daycode = ( daycode3 + days_in_month[month3] ) % 7;
	}
}

void tolowermonthname(char str[]){
  int i = 0;

  while(str[i] != '\0'){
    str[i] = tolower(str[i]);
    i++;
  }
}

int getmonthnumber(char *month){
  if(strcmp(month,"january")==0 || strcmp(month,"jan")==0) return 1;
  else if(strcmp(month,"february")==0 || strcmp(month,"feb")==0) return 2;
  else if(strcmp(month,"march")==0 || strcmp(month,"mar")==0) return 3;
  else if(strcmp(month,"april")==0 || strcmp(month,"apr")==0) return 4;
  else if(strcmp(month,"may")==0) return 5;
  else if(strcmp(month,"june")==0 || strcmp(month,"jun")==0) return 6;
  else if(strcmp(month,"july")==0 || strcmp(month,"jul")==0) return 7;
  else if(strcmp(month,"august")==0 || strcmp(month,"aug")==0) return 8;
  else if(strcmp(month,"september")==0 || strcmp(month,"sept")==0) return 9;
  else if(strcmp(month,"october")==0 || strcmp(month,"oct")==0) return 10;
  else if(strcmp(month,"november")==0 || strcmp(month,"nov")==0) return 11;
  else if(strcmp(month,"december")==0 || strcmp(month,"dec")==0 ) return 12;
  else return 0;
}

void show_month_calendar(int month){
    int year = time_systime.year;
    int daycode = determine_first_day(year);
    int day, i;
    int days_in_month[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    char *months[]= {
      " ",
      "January",
      "February",
      "March",
      "April",
      "May",
      "June",
      "July",
      "August",
      "September",
      "October",
      "November",
      "December"
    };


   for (i=1; i<=12; i++){ // loop through all the months of the current year to have a correct daycode.
      if(i==month){           
        textcolor(YELLOW);
        printf("%s", months[i]);
        textcolor(WHITE);
        printf("\nSu Mo Tu We Th Fr Sa\n" );
         
        // Correct the position for the first date
        if(daycode!=0){
          for(day=1; day<=1+daycode*2.9; day++)
            printf(" ");
        }

        // Print all the dates for one month
        for (day=1; day<=days_in_month[i]; day++){
          if(day == time_systime.day && i == time_systime.month && year == time_systime.year)
            textcolor(GREEN);
          else textcolor(WHITE);
            printf("%2d", day);

          // Is day before Sat? Else start next line Sun.
          if ((day+daycode)% 7 > 0 ) printf(" ");
          else printf("\n" );
        }
      
        printf("\n");
      }
      // Set position for next month
      daycode = ( daycode + days_in_month[i] ) % 7;      
   }
}

/* ==================================================================
   console_execute(const char *str):
   * This command is used to execute a console string.

*/
int console_execute(const char *str)
{
  char temp[512];
  char *u;
  int command_length = 0;
  signed char mouse_x,mouse_y,last_mouse_x=0,last_mouse_y=0;
  
  //make a copy so that strtok wouldn't ruin str
  strcpy(temp,str);
  u=strtok(temp," ");
  
  if (u==0) return;
  
  command_length = strlen(u);    
    
    //check if a pathcut command was executed
    if (u[command_length - 1] == ':') 
                {
                    char temp[512];
                    sprintf(temp,"cd %s",u);            
                    console_execute(temp); 
                }
                else
    if (strcmp(u,"fgman")==0)
                {
                    fg_set_state(1);
                }
                else
    if (strcmp(u,"mouse")==0)
                {
                  while (!kb_ready()){
                    get_mouse_pos(&mouse_x,&mouse_y);
                    printf("Mouse (x,y): %d %d\n",mouse_x, mouse_y);
                    while ((last_mouse_x == mouse_x) && (last_mouse_y==mouse_y))
                    {
                      get_mouse_pos(&mouse_x,&mouse_y);
                    }
                    last_mouse_x=mouse_x;
                    last_mouse_y=mouse_y; 
                  }
                }
                else
    if (strcmp(u,"shutdown")==0)
                {
                    sendmessage(0,MES_SHUTDOWN,0);
                }
                else
    if (strcmp(u,"procinfo")==0)
                {
                   int pid;             
                   u=strtok(0," ");
                   if (u!=0)
                   {
                       pid = atoi(u);
                       show_process_stat(pid);
                   };
                }
                else
    if (strcmp(u,"meminfo")==0)
                mem_interpretmemory(memory_map,map_length);
                else
    if (strcmp(u,"pause")==0)
                {
                printf("press any key to continue or 'q' to quit..\n");
                if (getch()=='q') return -1;
                }
                else
    if (strcmp(u,"lspcut")==0)
                vfs_showpathcuts();
                else
    if (strcmp(u,"pcut")==0)
                {
                   char *u2,*u3;
                   u2 = strtok(0," ");
                   u3 = strtok(0," ");
                   if (u2!=0)
                      {
                          if (vfs_addpathcut(u2,u3)==-1)
                            printf("Invalid Pathcut specified\n");
                              else
                            printf("pathcut added.\n");                                
                      }
                   else
                      printf("Wrong number of parameters specified\n");
                }
                else
    if (strcmp(u,"rmdir")==0)
                {
                char *u2 = strtok(0," ");
                if (u2!=0)
                    {
                        char c;                
                        printf("*Warning!* This will delete all files and subdirectories!\n");
                        printf("Do you wish to continue? (y/n):");
                        c = getch();
                        if (c == 'y')
                           {
                              printf("please wait..\n");                        
                              if (rmdir(u2)!=-1)
                                printf("Remove directory successful!\n");
                              else
                                printf("Error while removing directory.\n");
                           }
                        else
                           printf("Remove directory cancelled.\n");
                    }
                   else
                   printf("Invalid parameter.\n"); 
                }
                else
    if (strcmp(u,"rempcut")==0)
                {
                   char *u2;
                   u2 = strtok(0," ");
                   if (u2!=0)
                     {
                     if (vfs_removepathcut(u2)==-1)
                        printf("Invalid Pathcut or pathcut not found\n");
                           else
                        printf("pathcut removed.\n");   
                     
                     }
                   else
                     printf("Wrong number of parameters specified\n");
                     
                }
                else
    if (strcmp(u,"newconsole")==0)
                {
                //create a new console         
                   console_new();
                   printf("new console thread created.\n");                   
                }
                else  
    if (strcmp(u,"ver")==0) {
		printf("%s",dex32_versionstring);
                }
                else
    if (strcmp(u,"cpuid")==0)
                {
                   hardware_cpuinfo mycpu;
                   hardware_getcpuinfo(&mycpu);
                   hardware_printinfo(&mycpu);
                }
                else            
    if (strcmp(u,"exit")==0)
                {
                  fg_exit();
                  exit(0);              
                }
                else  
    if (strcmp(u,"echo")==0)
                {
                  u=strtok(0,"\n");
                  
                  if (u!=0)              
                      printf("%s\n",u);
                }
                else  
    if (strcmp(u,"use")==0)
                {
                u=strtok(0," ");
                if (extension_override(devmgr_getdevicebyname(u),0)==-1)
                    {
                        printf("Unable to install extension %s.\n",u);                
                    };            
                }
                else        
    if (strcmp(u,"off")==0)
                dex32apm_off();
                else
    if (strcmp(u,"files")==0)
                 file_showopenfiles();
                else
    if (strcmp(u,"find")==0)
                {
                u=strtok(0," ");
                if (u!=0)
                  findfile(u);
                }
              else
    if (strcmp(u,"kill")==0)
                {
                u=strtok(0," ");
                if (u!=0)
                      dex32_killkthread_name(u);
                }
                else
    if (strcmp(u,"procs")==0||strcmp(u,"ps")==0)
                show_process();
                else
    if (strcmp(u,"cls")==0)
               clrscr();
                else
    if (strcmp(u,"help")==0)
               //console_execute("type /icsos/icsoshlp.txt");
               console_execute("type /icsos/icsoshlp.txt");
                else
    if (strcmp(u,"umount")==0)
             {
               char *u =strtok(0," ");
               if (u!=0)
                {
                 if (vfs_unmount_device(u)==-1)
                    printf("umount failed.\n");
                 else
                   printf("%s umounted.\n",u);
                } 
                  else
                printf("missing parameter.\n");                    
              }
              else
   if (strcmp(u,"mount")==0)
               {
               char *fsname,*devname,*location;
               fsname=strtok(0," ");
               devname=strtok(0," ");
               location=strtok(0," ");
               
               if (vfs_mount_device(fsname,devname,location)==-1)
                 printf("mount not successful.\n");
                   else
                 printf("mount successful.\n");  
               //fat12_mount_root(root,floppy_deviceid);
               }
              else
    if (strcmp(u,"path")==0)
               {
                char temp[255];
                printf("%s\n",showpath(temp));
               }
                else
    if (strcmp(u,"lsmod")==0)
                showlibinfo();
                else
    if (strcmp(u,"mem")==0)
                meminfo();
                else
    if (strcmp(u,"mkdir")==0)
               {
                u=strtok(0," ");
                if (u!=0)
                   if (mkdir(u)==-1)
                     printf("mkdir failed.\n");
               }
                else       
    if (strcmp(u,"run")==0)
               {
               u=strtok(0," ");
               if (u!=0)
                  {
                  if (script_load(u)==-1)
                      {
                         printf("console: Error loading script file.\n");
                      };            
                  }
               }
               else    
    if (strcmp(u,"ls")==0||strcmp(u,"dir")==0)
               {
               int style=0, ordering = 0;
               
               u=strtok(0," ");
               
               if (u!=0)
               {
                   do {           
                   if (strcmp(u,"-l")==0) style=1;
                   if (strcmp(u,"-oname")==0) ordering  = 0;
                   if (strcmp(u,"-osize")==0) ordering  = 1;
                   u=strtok(0," ");
                   } while (u!=0);
               };
               
               console_ls(style, ordering);
               }
              else
     if (strcmp(u,"del")==0)
              {
              int res;
               u=strtok(0," ");
               if (u!=0)
                  {
                           char *u3=strtok(0," ");
                           if (u3==0)
                           {
                               delfile(u);
                               printf("file deleted.\n");
                           }
                            else
                           printf("invalid parameter.\n");

                  }
                     else
                  printf("missing parameter.\n");

              }

              else
    if (strcmp(u,"ren")==0)
              {
              char *u2,*u3;
               u2=strtok(0," ");
               u3=strtok(0," ");               
               if (u2!=0&&u3!=0)
                  {
                     if (rename(u2,u3)) printf("file renamed.\n");
                        else
                         printf("error renaming file.\n");
                  }
                    else
                printf("missing parameter.\n");    
              }
              else
    if ( strcmp(u,"type")==0 || strcmp(u,"cat")==0 )
              {
               u=strtok(0," ");
               if (u!=0)
                  {
                   if (console_showfile(u,0)==-1)
                    printf("error opening file.\n");
                  }
                  else
               printf("missing parameter.\n");

              }
              else
/*============================== CMSC 125 Project ==============================*/
    if (strcmp(u, "cal") == 0) {
    	int year = 0;
      char month[20];
      int monthnumber;
    	
    	// get arguments
    	u=strtok(0, " ");

    	// check if there are arguments
    	if (u!=0){
    		// if argument is year
    		if(strcmp(u, "-y")==0 || strcmp(u, "-year")){
    			u = strtok(0, " "); // get next argument

    			// check if there is one
    			if (u!=0){
    				year = atoi(u);
    				if(year < 1800){
    					printf("Invalid argument! Range must be from year 1800\n");
    				}else{
    					show_year_calendar(year);
    				}
    			}
    			else{
    				printf("cal: option requires an argument\n");
    			}
    		} else if(strcmp(u, "-m")==0 || strcmp(u, "-month")){
            u = strtok(0, " ");

            if(u!=0){
               if(atoi(u)==0){   
                  tolowermonthname(u);
                  monthnumber = getmonthnumber(u);
                  if(monthnumber == 0 || monthnumber > 12)
                     printf("cal: %s is neither a month number (1..12) nor a name\n", month);
               } else {
                  monthnumber = atoi(u);
                  if(monthnumber == 0 || monthnumber > 12)
                     printf("cal: %d is neither a month number (1..12) nor a name\n", monthnumber);
               }

               if(monthnumber > 0 && monthnumber <=12){
                  show_month_calendar(monthnumber);
               }

            }
            else {
               printf("cal: option requires an argument\n");
            }
         }
         else { // no arguments, display current month of the current year
            show_month_calendar(time_systime.month);
         }

    	}
   	
   	}
    	else
/*============================== CMSC 125 Project ==============================*/
   if (strcmp(u,"copy")==0)
              {
              u=strtok(0," ");
              if (u!=0) 
                   {
                      char *u2 = strtok(0," ");
                      if (u2!=0) 
                          {
                            if (fcopy(u,u2) == -1)
                                {
                                    printf("copy failed. Error while copying. Destination\n");
                                    printf("directory might not be present.\n");
                                };
                          };  
                   };
              }
              else              
   if (strcmp(u,"cd")==0)
              {
              u=strtok(0," ");
              if (u!=0)  {
               if (!changedirectory(u))
               printf("cd: Cannot find directory\n");
                         } else
                changedirectory(0); //go to working directory 
              } else
   if (strcmp(u,"loadmod")==0)
              {
               u=strtok(0," ");
               if (u!=0)
                {
               if (loadDLL(u,str)==-1)
                  printf("Unable to load %s.\n",u);
                    else
                  printf("Load module Successful.\n");  
                }
                  else
               printf("missing parameter.\n");

              }
              else
    if (strcmp(u,"lsdev")==0)
             {
              devmgr_showdevices();
             }
             else
    if (strcmp(u,"lsext")==0)
    			 {
              extension_list();
             }
             else
    if (strcmp(u,"libinfo")==0)
             {
              u=strtok(0," ");
              module_listfxn(u);
             }
             else
    if (strcmp(u,"time")==0)
             {
             printf("%d/%d/%d %d:%d.%d (%d total seconds since 1970)\n",time_systime.day,
                      time_systime.month, time_systime.year,
                      time_systime.hour, time_systime.min,
                      time_systime.sec,time());
             }
             else
    if (strcmp(u,"set")==0)
             {
              u=strtok(0," ");
              if (u==0)
                 {
                   env_showenv();
                 }
                  else
                 {
                    char *name  = strtok(u,"=");
                    char *value = strtok(0,"\n");
                    env_setenv(name,value,1);
                 }; 
             }
             else         
    if (strcmp(u,"unload")==0)
    			 {
             u=strtok(0," ");
             if (u!=0)
             	{
	             if (module_unload_library(u)==-1)
                printf("Error unloading library");
   	         };
             }
             else
    if (strcmp(u,"demo_graphics")==0)
              {
               demo_graphics();
              }
    
              else
    if (u[0]=='$')
             {
               int i, devid;
               char devicename[255],*cmd;
               for (i=0;i<20&&u[i+1];i++)
                    {
                      devicename[i] = u[i+1];
                    };
                    
               devicename[i] = 0;
               
               printf("sending command to %s\n",devicename);
               devid = devmgr_finddevice(devicename);
               
               if (devid != -1)
               {
               if (devmgr_sendmessage(devid,DEVMGR_MESSAGESTR,str)==-1)
                   printf("console: send_message() failed or not supported.\n");
               }
                  else
               printf("console: cannot find device.\n");   

             }
             else
             {
               if (u!=0)
                 {
                  
                  if (!user_execp(u,0,str))
                      printf("console32: undefined console command.\n");

                 }

              }
//normal termination
return 1;
};

int console_new()
{
         //create a new console         
         char consolename[255];
         sprintf(consolename,"dex32_console(%d)",console_first);    
         return createkthread((void*)console,consolename,200000);
};

void console_main()
  {
    DEX32_DDL_INFO *myddl=0;
    fg_processinfo *myfg;
    char s[256]="";
    char temp[256]="";
    char last[256]="";
    char console_fmt[256]="%cdir% %% ";
    char console_prompt[256]="cmd >";
    
    DWORD ptr;
    
    myddl =Dex32CreateDDL();    

    
    Dex32SetProcessDDL(myddl,getprocessid());
    myfg = fg_register(myddl,getprocessid());
    fg_setforeground( myfg->id );


    clrscr();
    
    
          
    strcpy(last,"");
    
    
    if (console_first == 0) script_load("/icsos/autoexec.bat");
    
    console_first++;  
    do {
    textcolor(WHITE);
    textbackground(BLACK);
    prompt_parser(console_fmt,console_prompt);
    
    textcolor(LIGHTBLUE);
    printf("%s",console_prompt);
    textcolor(WHITE);
    
    if (strcmp(s,"@@")!=0&&
        strcmp(s,"!!")!=0)
    strcpy(last,s);
    
    getstring(s,myddl);
   
    if (strcmp(s,"!")==0)
               sendtokeyb(last,&_q);
               else
    if (strcmp(s,"!!")==0)
              {
               sendtokeyb(last,&_q);
               sendtokeyb("\r",&_q);
              }
               else   
    console_execute(s);
    } while (1);
  ;};

