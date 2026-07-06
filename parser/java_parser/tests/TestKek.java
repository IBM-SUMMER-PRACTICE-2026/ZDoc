public class TestKek {

    /********************************************************************
    * Method  :  Kek constructor
    * Function:  Creates a Kek object and loads the continuous
    *            data for the object
    *
    * @param  sadkek   Name of this continuous feature (String)
    * @param  kekw     Array of data for this continuous feature (int[])
    *********************************************************************/
    Kek(String sadkek, int [] kekw) {
        this.sadkek = sadkek;
        this.kekw = kekw;
    }

    // No doc comment above this one - must not appear in output.
    void doStuff() {
        System.out.println("stuff");
    }

    /********************************************************************
    * Method  :  loadKekData
    * Function:  Loads Kek data from a file on disk into this object,
    *            validating the format before storing it
    *
    * @param  path   Path to the file to load. Must be a readable,
    *                absolute path to an existing file on disk
    * @return true if the data was loaded successfully, false otherwise
    * @throws IllegalArgumentException if path is null or empty
    * @throws java.io.IOException if the file cannot be read
    *********************************************************************/
    @SuppressWarnings("unchecked")
    boolean loadKekData(String path) throws java.io.IOException {
        return true;
    }
}
