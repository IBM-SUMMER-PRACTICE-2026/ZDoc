 /********************************************************************/
 /* no_doc_comments.plx                                          @L0A*/
 /*                                                              @L0A*/
 /* Regression fixture: valid PL/X procedures with ordinary code @L0A*/
 /* comments only - NO structured doc-comment blocks. The parser @L0A*/
 /* should extract zero documented symbols from this file.       @L0A*/
 /********************************************************************/

 COMPUTE_TOTAL: PROC(A, B) RETURNS(FIXED(31)); /* start           @L0A*/
   DCL A FIXED(31);                 /* first operand              @L0A*/
   DCL B FIXED(31);                 /* second operand             @L0A*/
   DCL SUM FIXED(31);               /* running total              @L0A*/

   SUM = A + B;                     /* add the two operands       @L0A*/
   RETURN(SUM);                     /* hand the result back       @L0A*/
 END COMPUTE_TOTAL; /* done                                       @L0A*/
 @EJECT;

 /* -- helper that clamps a value into the 0..100 range -- */
 CLAMP_SCORE: PROC(SCORE) RETURNS(FIXED(31));
   DCL SCORE FIXED(31);             /* candidate score            @L0A*/

   IF SCORE < 0 THEN                /* below the floor            @L0A*/
      SCORE = 0;
   IF SCORE > 100 THEN              /* above the ceiling          @L0A*/
      SCORE = 100;
   RETURN(SCORE);
 END CLAMP_SCORE;
 @EJECT;

 RESET_COUNTERS: PROC;             /* no parameters, no doc block  @L0A*/
   DCL I FIXED(31);
   DO I = 1 TO 10;                  /* zero every slot            @L0A*/
      COUNTERS(I) = 0;
   END;
 END RESET_COUNTERS;
