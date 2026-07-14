 /********************************************************************/
 /* basic_doc.plx                                                    */
 /* Small, deterministic fixture: one documented procedure and one   */
 /* undocumented one, for unit-test assertions.                      */
 /********************************************************************/

 /****************************************************************** */
 /* TITLE: ADD_NUMBERS                                                */
 /* FUNCTION: Adds two integers and returns the sum                  */
 /* INPUT: A - first operand                                          */
 /* INPUT: B - second operand                                         */
 /* OUTPUT: Sum of A and B                                            */
 /****************************************************************** */
 ADD_NUMBERS: PROC(A, B) RETURNS(FIXED(31));
   DCL A FIXED(31);
   DCL B FIXED(31);
   RETURN(A + B);
 END ADD_NUMBERS;
 @EJECT;

 /* plain comment, not a doc-comment block - must not attach */
 UNDOCUMENTED_PROC: PROC;
   DCL X FIXED(31);
   X = 0;
 END UNDOCUMENTED_PROC;
