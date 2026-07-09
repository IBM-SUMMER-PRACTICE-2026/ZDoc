 @PROCESS
      TITLE('TUES Summer Practices - Executable')
      ENV(z/OS)
      Mach(zS-4)                                             /* @00C*/
      GMAR(1,72)
      OPT(ADV,64)
      REORDER
      INLINE(MAX)
      COMPILE(N)
      ;                                                      /* @L0A*/
 @PROCESS OVERRIDE FORMAT NOIMP(0FX);                        /* @00C*/
 /****** START OF SPECIFICATIONS *************************************/
 /*                                                                  */
 /*01*  MODULE-NAME = student_grades                                 */
 /*                                                                  */
 /*01*  CSECT NAME = student_grades                                  */
 /*                                                                  */
 /*01*  DESCRIPTIVE-NAME = TUES Summer Practices - Executable        */
 /*                                                                  */
 /*01*  FUNCTION =                                                   */
 /*                                                                  */
 /*       This module makes use of methods and constants, exported   */
 /*       for use by student_grades macro.                           */
 /*                                                                  */
 /*02*  OPERATION =                                                  */
 /*                                                                  */
 /*      INITIALIZE_SYSTEM                                           */
 /*         - Initialization logic for use in the module             */
 /*                                                                  */
 /*      ADD_STUDENT                                                 */
 /*         - Adds a student entity to an array of students          */
 /*                                                                  */
 /*      ENTER_GRADES                                                */
 /*         - Modify student data by adding grades to their record   */
 /*                                                                  */
 /*      FIND_STUDENT_BY_ID                                          */
 /*         - Finds a specific student by passing an ID as argument  */
 /*                                                                  */
 /*      SORT_STUDENTS_BY_AVERAGE                                    */
 /*         - Sort every student based on their average grade        */
 /*                                                                  */
 /*      BINARY_SEARCH_BY_ID                                         */
 /*         - If students are sorted by ID, optimize searching by    */
 /*           implementing binary search                             */
 /*                                                                  */
 /*      CALCULATE_CLASS_STATISTICS                                  */
 /*         - Calculate good-to-know statistics of all the students  */
 /*                                                                  */
 /*      DISPLAY_STUDENT                                             */
 /*         - Display detailed information of a student              */
 /*                                                                  */
 /*      DISPLAY_ALL_STUDENTS                                        */
 /*         - Display information about all of the students          */
 /*                                                                  */
 /*02*  CHARACTER-CODE-DEPENDENCIES =                                */
 /*     Any character code other than EBCDIC will require            */
 /*     re-assembly of this module.                                  */
 /*                                                                  */
 /*02*  RESTRICTIONS = None                                          */
 /*                                                                  */
 /*02*  REGISTER-CONVENTIONS = See Register DECLARES section         */
 /*                                                                  */
 /*03*  REGISTERS-SAVED = R0->R14                                    */
 /*                                                                  */
 /*03*  REGISTERS-USAGE =                                            */
 /*     R8  - data register                                          */
 /*     R13 - data register                                          */
 /*     R12 - stat register                                          */
 /*                                                                  */
 /*03*  REGISTERS-RESTORED = R0->R14                                 */
 /*                                                                  */
 /*02*  PATCH-LABEL = None                                           */
 /*                                                                  */
 /*01*  MODULE-TYPE = Procedure                                      */
 /*                                                                  */
 /*02*  PROCESSOR = PL/X                                             */
 /*                                                                  */
 /*02*  MODULE-SIZE = See assembled length                           */
 /*                                                                  */
 /*01*  ENTRY-POINT = STUDENT_GRADES                                 */
 /*                                                                  */
 /*02*  PURPOSE = See FUNCTION.                                      */
 /*                                                                  */
 /*03*  ENTRY REGISTERS =                                            */
 /*             REGISTER 0-12 = N/A                                  */
 /*             REGISTER 13   = Address of save area                 */
 /*             REGISTER 14   = Return address                       */
 /*             REGISTER 15   = N/A                                  */
 /*                                                                  */
 /*01*  INPUT = None                                                 */
 /*                                                                  */
 /*01*  OUTPUT = None                                                */
 /*                                                                  */
 /*01*  EXIT-NORMAL = This is a never ending task.                   */
 /*                                                                  */
 /*02*  CONDITIONS = N/A                                             */
 /*                                                                  */
 /*03*  EXIT-REGISTERS = N/A                                         */
 /*                                                                  */
 /*02*  RETURN-CODES = None                                          */
 /*03*  EXIT-REGISTERS = R0->R14 restored.                           */
 /*                                                                  */
 /*02*  RETURN-CODES = None.                                         */
 /*                                                                  */
 /*02*  DATA-AREAS = None.                                           */
 /*                                                                  */
 /*02*  CONTROL-BLOCKS =                                             */
 /*     NAME      MAPPING MACRO   REFERENCE                          */
 /*     ----      -------------   ---------                          */
 /*     STG       student_grades  R                                  */
 /*                                                                  */
 /* KEY = C - Create, D - Delete, M - Modify, R - Read               */
 /*                                                                  */
 /*01*  TABLES = None.                                               */
 /*                                                                  */
 /*01*  CHANGE-ACTIVITY =                                            */
 /* $MOD(STUDENT_GRADES),COMP(TSS): TUES Summer Practices - Exec     */
 /*                                                                  */
 /* $L0=feature1,version1,date1,initials1 : Add initial methods for  */
 /*                                         initialization and       */
 /*                                         insertion of grades and  */
 /*                                         students                 */
 /* $L1=feature2,version2,date2,initials2 : Add procedures for       */
 /*                                         sorting and searching of */
 /*                                         students                 */
 /* $00=fix1,    version1,date3,initials3 : Fix file config options  */
 /* $L2=feature, version2,date4,initials1 : Add calculation and      */
 /*                                         display methods          */
 /*                                                                  */
 /********************************************************************/
 /******************=> END OF SPECIFICATIONS <=***********************/
 /********************************************************************/
 /* Change Activity:                                                 */
 /* ----------------                                                 */
 /*                                                                  */
 /* A - Add initial methods for initialization and insertion of  @L0A*/
 /*     grades and students                                      @L0A*/
 /* A - Add procedures for sorting and searching of students     @L1A*/
 /* C - Fix file config options                                  @00A*/
 /* A - Add calculation and display methods                      @L2A*/
 /*                                                                  */
 /********************************************************************/
 @EJECT;
 STUDENT_GRADES: PROC 
                  OPTIONS(
                     REENTRANT,           /* Reentrant - Yes     @L0A*/
                     AMODE(31),         /* Addressing mode - 31  @00C*/
                     RMODE(ANY),        /* Residency mode  - Any @00C*/
                     ?ID(STUDENT_GRADES);,/* Id qualifier        @L0A*/
                     NOPATCHAREA,         /* Patcharea - No      @L0A*/
                     StatReg(REG12),      /* Static area - R12   @L0A*/
                     CodeReg(*),          /* Code addressability @L0A*/
                     DATAREGX(REG13) /* Dyn area addressability  @L0A*/
                     );                   /*                         */
 @EJECT;

  % INCLUDE SYSLIB(student_grades); /* student_grades macro      @L0A*/
 @EJECT;
 /* Constants                                                    @L1A*/
 DCL NOT_FOUND_CONSTANT FIXED(31) CONSTANT(-1); /* Return value if
                                             not founf           @L1A*/
 @EJECT;
 /* Global variables                                             @L0A*/
 DCL STUDENT_ARRAY(MAX_STUDENTS) ISA(STUDENT_RECORD);  /* Student 
                                             array               @L0A*/
 DCL STUDENT_COUNT FIXED(31) SINIT(0); /* Current student count  @L0A*/
 DCL (I, J) FIXED(31);             /* Loop variables             @L2A*/
 DCL RC FIXED(31);                 /* Return code                @L0A*/
 @EJECT;
 /********************************************************************/
 /*                                                              @L0A*/
 /*  Title: INITIALIZE_SYSTEM:                                   @L0A*/
 /*                                                              @L0A*/
 /*  Logic: Initialize the student management system             @L0A*/
 /*                                                              @L0A*/
 /*  Input: None                                                 @L0A*/
 /*                                                              @L0A*/
 /*  Output: None                                                @L0A*/
 /*                                                              @L0A*/
 /********************************************************************/
 INITIALIZE_SYSTEM: PROC; /* Start INITIALIZE_SYSTEM             @L0A*/
   DCL I FIXED(31);                          /* Temporary variable for
                                       looping                   @L0A*/
   
   /* Initialize all elements to null                            @L0A*/
   DO I = 1 TO MAX_STUDENTS;                 /* Loop through all 
                                             students            @L0A*/
      STUDENT_ARRAY(I) = ''b;                /* Set field to empty 
                                                                 @L0A*/
   END;                                      /* End. Loop through all 
                                             students            @L0A*/
   
   CALL PRINT('System initialized successfully'); /* Informational
                                          message on success     @L0A*/
 END INITIALIZE_SYSTEM; /* End INITIALIZE_SYSTEM                 @L0A*/
 @EJECT;
 /********************************************************************/
 /*                                                              @L0A*/
 /*  Title: ADD_STUDENT:                                         @L0A*/
 /*                                                              @L0A*/
 /*  Logic: Add a new student to the system                      @L0A*/
 /*                                                              @L0A*/
 /*  Input: ID, Name, Year                                       @L0A*/
 /*                                                              @L0A*/
 /*         Where ID is:                                         @L0A*/
 /*                                                              @L0A*/
 /*           1) Fixed(31) number                                @L0A*/
 /*                                                              @L0A*/
 /*         Where Name is:                                       @L0A*/
 /*                                                              @L0A*/
 /*           1) A character array with maximum length of 30     @L0A*/
 /*                                                              @L0A*/
 /*         Where Year is:                                       @L0A*/
 /*                                                              @L0A*/
 /*           1) Fixed(31) number                                @L0A*/
 /*                                                              @L0A*/
 /*  Output: Return code -                                       @L0A*/
 /*                                                              @L0A*/
 /*            0  - RC_SUCCESS                                   @L0A*/
 /*                                                              @L0A*/
 /*            12 - RC_ARRAY_FULL                                @L0A*/
 /*                                                              @L0A*/
 /********************************************************************/
 ADD_STUDENT: PROC(ID, NAME, YEAR) 
               RETURNS(FIXED(31)); /* Start ADD_STUDENT          @L0A*/
   DCL ID FIXED(31);               /* ID of student              @L0A*/
   DCL NAME CHAR(30);              /* Name of the student        @L0A*/
   DCL YEAR FIXED(31);             /* Year of birth date for 
                                       student                   @L0A*/
   DCL I FIXED(31);                /* Temporary variable for
                                       looping                   @L0A*/
   
   /* Check if array is full                                         */
   IF STUDENT_COUNT >= MAX_STUDENTS THEN /* Is array full ?      @L0A*/
   DO;                                   /* Yes.                 @L0A*/
      CALL PRINT('Error: Maximum students reached'); /* Debug
                                             message if students
                                             array is full       @L0A*/
      RETURN RC_ARRAY_FULL;              /* Array full return 
                                          code                   @L0A*/
   END;                                  /* End. Is array full ? @L0A*/

   /* Add to array                                               @L0A*/
   STUDENT_COUNT = STUDENT_COUNT + 1;    /* Increase count of
                                          added students         @L0A*/
   
   /* Initialize student data                                    @L0A*/
   STUDENT_ARRAY(STUDENT_COUNT).STUDENT_ID   = ID; /* Set 
                                                student id       @L0A*/
   STUDENT_ARRAY(STUDENT_COUNT).STUDENT_NAME = NAME; /* Set 
                                                student name     @L0A*/
   STUDENT_ARRAY(STUDENT_COUNT).YEAR         = YEAR; /* Set
                                                student birth
                                                year             @L0A*/
   STUDENT_ARRAY(STUDENT_COUNT).AVERAGE      = 0; /* Set initial
                                                value of average @L0A*/
   STUDENT_ARRAY(STUDENT_COUNT).STATUS_FLAGS = STATUS_ACTIVE; /*
                                                Set student flags
                                                                 @L0A*/
   
   /* Initialize all grades to 0                                 @L0A*/
   DO I = 1 TO MAX_SUBJECTS;             /* Loop through all 
                                          students               @L0A*/
      STUDENT_ARRAY.GRADES(I) = 0;       /* Set default value 
                                          to zero                @L0A*/
   END;                                  /* End. Loop through all 
                                          students               @L0A*/
   
   CALL PRINT('Student added successfully'); /* Debug message for 
                                          insertion of student   @L0A*/
   RETURN RC_SUCCESS;                    /* Return success code  @L0A*/
 END ADD_STUDENT; /* End ADD_STUDENT                             @L0A*/
 @EJECT;
 /********************************************************************/
 /*                                                              @L0A*/
 /* Routine : ENTER_GRADES                                       @L0A*/
 /*                                                              @L0A*/
 /* Function: Enter grades for a student                         @L0A*/
 /*                                                              @L0A*/
 /* Input   : Student ID - Fixed(31)                             @L0A*/
 /*           Array of 5 grades - Fixed(31)                      @L0A*/
 /*                                                              @L0A*/
 /* Output  : Returns if a student is found or not               @L0A*/
 /*           RC_SUCCESS or RC_NOT_FOUND                         @L0A*/
 /*                                                              @L0A*/
 /********************************************************************/
 ENTER_GRADES: PROC(ID, GRADE_ARRAY) RETURNS(FIXED(31));
 /* Start ENTER_GRADES                                           @L0A*/
   DCL ID FIXED(31);                /* Identification of the 
                                    specified student            @L0A*/
   DCL GRADE_ARRAY(5) FIXED(31);    /* Student grades array      @L0A*/
   DCL FOUND_INDEX FIXED(31);       /* Student to be changed     @L0A*/
   DCL I FIXED(31);                 /* Temporary variable for
                                       looping                   @L0A*/
   DCL SUM FIXED(31);               /* Sum of grades             @L0A*/
   
   /* Find student by ID                                         @L0A*/
   FOUND_INDEX = FIND_STUDENT_BY_ID(ID); /* Search for student 
                                                by ID            @L1C*/
   
   IF FOUND_INDEX = NOT_FOUND_CONSTANT THEN /* Is student found? @L0A*/
   DO;                                      /* Yes               @L0A*/
      CALL PRINT('Error: Student not found'); /* Debug message if 
                                                student has not been
                                                found            @L0A*/
      RETURN RC_NOT_FOUND;                  /* Return not found code
                                                                 @L0A*/
   END;                                /* Is student found? End  @L0A*/
   
   /* Enter grades and calculate sum                             @L0A*/
   SUM = 0;                         /* Set initial sum to zero   @L0A*/
   DO I = 1 TO MAX_SUBJECTS;        /* Loop through student 
                                       grades                    @L0A*/
      STUDENT_ARRAY.GRADES(I) = GRADE_ARRAY(I); /* Set grade in 
                                             cell                @L0A*/
      SUM = SUM + GRADE_ARRAY(I);   /* Add grade to sum          @L0A*/
   END;                             /* End Loop through student 
                                       grades                    @L0A*/
   
   STUDENT_ARRAY(FOUND_INDEX).AVERAGE = SUM / MAX_SUBJECTS; /* Calculate 
                                                   average       @L0A*/
   
   /* Update honors status based on average                      @L0A*/
   IF STUDENT_ARRAY(FOUND_INDEX).AVERAGE >= GRADE_A THEN /* Check if
                                                average is higher
                                                than the highest 
                                                grade            @L0A*/
      DO;                           /* Average is higher         @L0A*/
         SET_STATUS(
            STUDENT_ARRAY(FOUND_INDEX).STATUS_FLAGS, STATUS_HONORS
            ); /* Set honors status                              @L0A*/
      END;                          /* End Average is higher     @L0A*/
   ELSE                             /* Average is lower than the
                                       highest grade             @L0A*/
      DO;                           /* Average is lower          @L0A*/
         CLEAR_STATUS(
            STUDENT_ARRAY(FOUND_INDEX).STATUS_FLAGS, STATUS_HONORS
            ); /* Clear student status                           @L0A*/
      END;                          /* End Average is lower      @L0A*/
   
   /* Set warning status if average is below D                   @L0A*/
   IF STUDENT_ARRAY(FOUND_INDEX).AVERAGE < GRADE_D THEN /* Check if
                                                average is lower
                                                than the lowest 
                                                grade            @L0A*/
      DO;                           /* Average is lower          @L0A*/
         SET_STATUS(
            STUDENT_ARRAY(FOUND_INDEX).STATUS_FLAGS, STATUS_WARNING
            ); /* Set warning status                             @L0A*/
      END;                          /* End Average is lower      @L0A*/
   ELSE                             /* Average is higher than 
                                       lowest grade              @L0A*/
      DO;                           /* Average is higher         @L0A*/
         CLEAR_STATUS(
            STUDENT_ARRAY(FOUND_INDEX).STATUS_FLAGS, STATUS_WARNING
            ); /* Clear student status                           @L0A*/
      END;                          /* End. Average is higher    @L0A*/
   
   CALL PRINT('Grades entered successfully'); /* Debug message if
                                       grades have been entered 
                                       successfully              @L0A*/
   RETURN RC_SUCCESS;               /* Return success code       @L0A*/
 END ENTER_GRADES; /* End ENTER_GRADES                           @L0A*/
 @EJECT;
 /****************************************************************** */
 /*                                                              @L1A*/
 /* TITLE: FIND_STUDENT_BY_ID:                                   @L1A*/
 /*                                                              @L1A*/
 /* FUNCTION: Search for student by ID (Linear Search)           @L1A*/
 /*                                                              @L1A*/
 /* INPUT: ID - FIXED(31)                                        @L1A*/
 /*                                                              @L1A*/
 /* OUTPUT: Index - FIXED(31)                                    @L1A*/
 /*                                                              @L1A*/
 /*            -1 - If not found                                 @L1A*/
 /*                                                              @L1A*/
 /****************************************************************** */
 /****************************************************************** */
 /*++h 'FIND_STUDENT_BY_ID': ENTRY                               @L1A*/
 /****************************************************************** */
 FIND_STUDENT_BY_ID: PROC(ID) 
                     RETURNS(FIXED(31)); 
 /* Start FIND_STUDENT_BY_ID                                     @L1A*/
   DCL ID FIXED(31);                      /* Student ID          @L1A*/
   DCL I FIXED(31);                       /* Temporary variable for
                                             looping             @L1A*/
   
   /* Linear search through array                                @L1A*/
   DO I = 1 TO STUDENT_COUNT;    /* Loop through students        @L1A*/
      IF STUDENT_ARRAY(I).STUDENT_ID = ID THEN /* Check if IDs 
                                                match            @L1A*/
         DO;                           /* IDs match              @L1A*/
            RETURN I;                  /* Return found index     @L1A*/
         END;                          /* End. IDs match         @L1A*/
   END;                          /* End. Loop through students   @L1A*/
   
   RETURN -1;                          /* Return not found       @L1A*/
 END FIND_STUDENT_BY_ID;
 /****************************************************************** */
 /*++h END 'FIND_STUDENT_BY_ID'                                  @L1A*/
 /****************************************************************** */
 @EJECT;
 /********************************************************************/
 /*                                                              @L1A*/
 /* Routine : SORT_STUDENTS_BY_AVERAGE                           @L1A*/
 /*                                                              @L1A*/
 /* Function: Sort students based on average                     @L1A*/
 /*                                                              @L1A*/
 /* Input   : None                                               @L1A*/
 /*                                                              @L1A*/
 /* Output  : None                                               @L1A*/
 /*                                                              @L1A*/
 /********************************************************************/
 SORT_STUDENTS_BY_AVERAGE: PROC;
 /* Start SORT_STUDENTS_BY_AVERAGE                               @L1A*/
   DCL I FIXED(31);                    /* Temporary variable for
                                             looping             @L1A*/
   DCL J FIXED(31);                    /* Temporary variable for
                                             looping             @L1A*/
   DCL SWAPPED BIT(1);                 /* Students swapped bit   @L1A*/
   DCL TEMP_STUDENT ISA(STUDENT_RECORD); /* Temporary student    @L1A*/
   
   /* Bubble sort algorithm                                      @L1A*/
   DO I = 1 TO STUDENT_COUNT - 1;      /* Outer loop through 
                                          students               @L1A*/
      SWAPPED = '0'B;                  /* Set swapped to false   @L1A*/
      
      DO J = 1 TO STUDENT_COUNT - I;   /* Inner loop through 
                                          students               @L1A*/
         IF (STUDENT_ARRAY(J).AVERAGE < 
               STUDENT_ARRAY(J + 1).AVERAGE) THEN /* Compare adjacent 
                                             elements            @L1A*/
            DO;                        /* Swap elements          @L1A*/
               TEMP_STUDENT = STUDENT_ARRAY(J); /* Set temp 
                                                student          @L1A*/
               STUDENT_ARRAY(J) = STUDENT_ARRAY(J + 1); /* Swap 
                                                students         @L1A*/
               STUDENT_ARRAY(J + 1) = TEMP_STUDENT; /* Assign next
                                                student to temp  @L1A*/
               SWAPPED = '1'B;                  /* Set swapped bit
                                                to true          @L1A*/
            END;                       /* End. Swap elements     @L1A*/
      END;                             /* End. Inner loop through 
                                          students               @L1A*/

      IF ^SWAPPED THEN                 /* Check if swaps 
                                          occurred               @L1A*/
         RETURN;                       /* Exit inner loop if 
                                          no swaps               @L1A*/
   END;                                /* End. Outer loop through 
                                          students               @L1A*/
   
   CALL PRINT('Students sorted by average (descending)'); /* Debug
                                          message if students
                                          have been sorted       @L1A*/
 /* End SORT_STUDENTS_BY_AVERAGE                                 @L1A*/
 END SORT_STUDENTS_BY_AVERAGE;
 @EJECT;
 /********************************************************************/
 /*                                                              @L1A*/
 /* Routine : BINARY_SEARCH_BY_ID                                @L1A*/
 /*                                                              @L1A*/
 /* Function: Search for a student by ID with binary search      @L1A*/
 /*                                                              @L1A*/
 /* Input   : None                                               @L1A*/
 /*                                                              @L1A*/
 /* Output  : Index of found index - Fixed(31)                   @L1A*/
 /*                                                              @L1A*/
 /********************************************************************/
 BINARY_SEARCH_BY_ID: PROC(ID) RETURNS(FIXED(31));
 /* Start BINARY_SEARCH_BY_ID                                    @L1A*/
   DCL ID FIXED(31);                   /* Student ID             @L1A*/
   DCL LEFT FIXED(31);                 /* Left pivot index       @L1A*/
   DCL RIGHT FIXED(31);                /* Right pivot index      @L1A*/
   DCL MID FIXED(31);                  /* Middle pivot index     @L1A*/
   
   LEFT = 1;                           /* Init left to first 
                                          index                  @L1A*/
   RIGHT = STUDENT_COUNT;              /* Init right to last 
                                          index                  @L1A*/
   
   DO WHILE (LEFT <= RIGHT);           /* Loop while left is less 
                                          than right             @L1A*/
      MID = (LEFT + RIGHT) / 2;        /* Calculate middle pivot @L1A*/
      
      IF STUDENT_ARRAY(MID).STUDENT_ID = ID THEN /* Is student found?
                                                                 @L1A*/
      DO;                              /* Yes                    @L1A*/
         RETURN MID;                   /* Return middle index    @L1A*/
      END;
      ELSE IF STUDENT_ARRAY(MID).STUDENT_ID < ID THEN DO; /* Is 
                                          student on the right side 
                                          of the array           @L1A*/
         LEFT = MID + 1;               /* Move left pivot to the
                                          right                  @L1A*/
      END;
      ELSE DO; /* Is student on the left side of the array       @L1A*/
         RIGHT = MID - 1;              /* Move right pivot to the
                                          left                   @L1A*/
      END;
   END;
   
   RETURN NOT_FOUND_CONSTANT; /* Return not found index          @L1A*/
 /* End BINARY_SEARCH_BY_ID                                      @L1A*/
 END BINARY_SEARCH_BY_ID;
 @EJECT;
 
 /****************************************************************** */
 /*                                                              @L2A*/
 /* TITLE: CALCULATE_CLASS_STATISTICS:                           @L2A*/
 /*                                                              @L2A*/
 /* FUNCTION: Calculate statistics of students in class          @L2A*/
 /*                                                              @L2A*/
 /* INPUT: None                                                  @L2A*/
 /*                                                              @L2A*/
 /* OUTPUT: None                                                 @L2A*/
 /*                                                              @L2A*/
 /****************************************************************** */
 /****************************************************************** */
 /*++h 'CALCULATE_CLASS_STATISTICS': ENTRY                       @L2A*/
 /****************************************************************** */
 CALCULATE_CLASS_STATISTICS: PROC;
 /* Start CALCULATE_CLASS_STATISTICS                             @L2A*/
   DCL (I                           /* Temp loop variable        @L2A*/
       ,TOTAL_AVG                   /* Average of all students   @L2A*/
       ,CLASS_AVG                   /* Average of students in 
                                       class                     @L2A*/
       ,HIGHEST_AVG                 /* Highest average 
                                       of students               @L2A*/
       ,LOWEST_AVG                  /* Lowest average
                                       of students               @L2A*/
       ,HONORS_COUNT                /* Count of all students 
                                       with honors status        @L2A*/
       ,WARNING_COUNT               /* Count of all students
                                       with warning status       @L2A*/
      )  FIXED(31);
   DCL IS_HONORS_FLAG   BIT(1);     /* Is honors flag bit set    @L2A*/
   
   IF STUDENT_COUNT = 0 THEN  /* Do students exist?              @L2A*/
   DO;                        /* No.                             @L2A*/
      CALL PRINT('No students in system'); /* Debug message if 
                                       students do not exist     @L2A*/
      RETURN;                       /* Return if no students 
                                       found                     @L2A*/
   END;                       /* End. Do students exist?         @L2A*/
   
   TOTAL_AVG = 0;                   /* Set total average to zero @L2A*/
   HIGHEST_AVG = 0;                 /* Set highest average to 
                                          zero                   @L2A*/
   LOWEST_AVG = 100;                /* Set lowest average to
                                          zero                   @L2A*/
   HONORS_COUNT = 0;                /* Set students with honors 
                                          status to zero         @L2A*/
   WARNING_COUNT = 0;               /* Set students with warning 
                                          status to zero         @L2A*/
   
   DO I = 1 TO STUDENT_COUNT;       /* Loop through all students @L2A*/
      TOTAL_AVG = TOTAL_AVG + STUDENT_ARRAY(I).AVERAGE; /* Sum the 
                                             total average of all 
                                             students            @L2A*/
      
      IF STUDENT_ARRAY(I).AVERAGE > HIGHEST_AVG THEN /* Is current 
                                             average higher than the 
                                             highest average     @L2A*/
         HIGHEST_AVG = STUDENT_ARRAY(I).AVERAGE; /* Update highest 
                                             average             @L2A*/
      
      IF STUDENT_ARRAY(I).AVERAGE < LOWEST_AVG THEN /* Is current 
                                             average lower than the 
                                             lowest average      @L2A*/
         LOWEST_AVG = STUDENT_ARRAY(I).AVERAGE; /* Update lowest 
                                             average             @L2A*/
      
      IS_HONORS(STUDENT_ARRAY(I).STATUS_FLAGS, IS_HONORS_FLAG); /*
                                             Check if student
                                             has honors status   @L2A*/
      
      IF IS_HONORS_FLAG THEN /* Is honors status set ?           @L2A*/
         HONORS_COUNT = HONORS_COUNT + 1; /* Increase honors 
                                                count            @L2A*/
      
      /* Check if student has a warning status                   @L2A*/
      IF (STUDENT_ARRAY(I).STATUS_FLAGS & STATUS_WARNING) ^= '00'B THEN
         WARNING_COUNT = WARNING_COUNT + 1; /* Increase warning 
                                                count            @L2A*/
   END;                             /* End. Loop through all 
                                          students               @L2A*/

   CLASS_AVG = TOTAL_AVG / STUDENT_COUNT; /* Calculate class 
                                       average of grades         @L2A*/
   
   CALL PRINT('=== CLASS STATISTICS ==='); /* Display statistics @L2A*/
   CALL PRINT('Total Students:', STUDENT_COUNT); /* Display total
                                                students         @L2A*/
   CALL PRINT('Class Average:', CLASS_AVG); /* Display class average
                                                                 @L2A*/
   CALL PRINT('Highest Average:', HIGHEST_AVG); /* Display highest 
                                                average          @L2A*/
   CALL PRINT('Lowest Average:', LOWEST_AVG); /* Display lowest 
                                                average          @L2A*/
   CALL PRINT('Honors Students:', HONORS_COUNT); /* Display count of 
                                                students with 
                                                honors status    @L2A*/
   CALL PRINT('Warning Students:', WARNING_COUNT); /* Display count of 
                                                students with 
                                                warning status   @L2A*/
 END CALCULATE_CLASS_STATISTICS;
 /****************************************************************** */
 /*++h END 'CALCULATE_CLASS_STATISTICS'                          @L2A*/
 /****************************************************************** */
 @EJECT;

 ?ASAEND DoIncludes;