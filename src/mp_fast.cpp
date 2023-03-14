// Includes
#include <stdint.h>
#include <hls_stream.h>
#include <ap_int.h>

#define DATA_SIZE 4096
// Largest X-image size (needed for buffering)
#define MAX_X 752
// Threshold for judging if intensity high enough. Better to do as variable, but want to avoid changing signature for now
#define THRESHOLD 20
// Rely on input data length being a multple of a power of 2, so that FPGA can load in bursts efficiently
// For initial test, will set to 64 (i.e. assuming cols_in is a multiple of 16). Would then expect input auto-widening.
#define BURSTLENGTH 64

//size of image is set here
#define MAX_SIZE 360960
// Max no of keypoints to output
#define MAX_OUT 5000
// TRIPCOUNT identifier
const int c_size = DATA_SIZE;

static void ReadFromMem(unsigned char* inputImage, hls::stream<unsigned char>& input_stream, int rows_in, int cols_in, int stride)
{
    // We assume that number of columns (x size) is multiple of 16.
    // If number of rows is not multiple of 4, then we will ignore the last few rows.
    int nblocks = rows_in / 4;
    int rows_to_read = nblocks * 4;
    // Given these assumptions, size will be a multiple of 32. For now, reading in bursts of 32 is probably fast enough.
    int size = rows_to_read * cols_in;
    // Manual recommends hinting that we have more than 0 data, for burst inference.
    assert (size > 0);

    // Use nested loop with fixed inner bound to allow FPGA to infer stuff like port widening, bursting etc.
    for (int i=0; i<size; i+=BURSTLENGTH) {
        for (int j=0; j<BURSTLENGTH; ++j) {
            // Pragma minimizes overhead of multiple loops when we have perfect loops (no code between loops, inner bound fixed)
            #pragma HLS loop_flatten
            #pragma HLS PIPELINE II = 1
            input_stream.write(inputImage[i+j]);
        }
    }
}

static void MPFASTprocess(hls::stream<unsigned char>& input_stream, int rows_in, int cols_in, int stride, hls::stream<int>& xkey_stream, hls::stream<int>& ykey_stream) {

    int xwidth = cols_in;

    //Use a single local buffer for data with a ring-buffer-like approach (we loop round and reuse initial rows, etc).
    //I.e. during our algorithm, y -> y % 4
    unsigned char loc_buff[4][MAX_X];

    //Array partitioning to disintegrate data into different memory blocks
    //Use CYCLIC partioning rather than block partition, because we need simultaneous access to NEARBY elements.
    //"Factor" controls number of blocks we subdivide into in each dimension. Here, cautiously using 8, but 4 should work in theory (each clock cycle, we only access data in a 4x4 region)
    #pragma HLS ARRAY_PARTITION variable=loc_buff cyclic factor=8 dim=0

    // Omit flag logic and dependencies between loops whereever possible; only thing to keep is the position where we write the output

    int nblocks = rows_in / 4; // Keep track of sets of 4 rows, for purposes of ring buffer logic

    for(int blockno = 0; blockno < nblocks; ++blockno){
        // Not sure if DEPENDENCE pragma should go here, or later
        for(int rowcount = 0; rowcount < 4; ++rowcount){
            for(int xpos = 0; xpos < xwidth; ++xpos){
                // Attempt to pipeline with one pixel per clock cycle
                #pragma HLS PIPELINE II=1
                // loop_flatten should reduce delay between loops, but this shouldn't be critical for performance - omit if problems occur
                #pragma HLS loop_flatten
                // Specify that there is no dependence in loc_buff between nearby loop iterations. (This is true because we read data from three lines while loading the next one.)
                // Not 100% sure where to put this directive when working with nested loops, but inner loop matters most for performance
                #pragma HLS dependence variable=loc_buff type=inter false

                // Load in new pixel data
                unsigned char pixdata = input_stream.read();
                loc_buff[rowcount][xpos] = pixdata;

                // Calculate relevant y positions
                int yloadpos = blockno * 4 + rowcount; // Y position the loaded data comes from
                int ycalcpos = yloadpos - 2; // 2 rows behind load position
                // Calculate relevant ring buffer y positions for the row being calculated, the next row and the previous row
                int ybuffcalc = (rowcount >= 2) ? rowcount - 2 : rowcount + 2;
                int ybuffnext = (rowcount >= 1) ? rowcount - 1 : rowcount + 3;
                int ybuffprev = (rowcount >= 3) ? rowcount - 3 : rowcount + 1;


                // Process pixel, unless we're at the start or end of a row, or at the beginning of the image
                if (xpos != 0 && xpos < (xwidth -1) && ycalcpos >= 1) {
                    // Copy value of pixel and neighbours; note ring buffer approach to y value.
                    unsigned char p = loc_buff[ybuffcalc][xpos];
                    unsigned char p1 = loc_buff[ybuffcalc][xpos+1];
                    unsigned char p2 = loc_buff[ybuffcalc][xpos-1];
                    unsigned char p3 = loc_buff[ybuffprev][xpos];
                    unsigned char p4 = loc_buff[ybuffnext][xpos];

                    bool threshtest = false;
                    threshtest = p < THRESHOLD;
                    // Check value relative to neighbour pixels. If more intense (lower value) than neighbour, value is incremented
                    int neighbourtest = 0;
                    neighbourtest += (p <= p1);
                    neighbourtest += (p <= p2);
                    neighbourtest += (p <= p3);
                    neighbourtest += (p <= p4);
                    // Repo version of the algorithm checks >=3. Is this correct? >=4 would give a local maximum, but >=3 only requires greater intensity than 3 neighbours
                    if ((neighbourtest >= 3) && threshtest) {
                        // Send data to output stream
                        // NB - Bug would occur if we exceed maximum allowed number of keypoints, since stream would overflow.
                        xkey_stream.write(xpos);
                        ykey_stream.write(ycalcpos);
                    }
                }
            }
        }
    }

    // At end of routine, send special values to indicate end of output.
    xkey_stream.write(-1);
    ykey_stream.write(-1);
    xkey_stream.write(-2);
    ykey_stream.write(-2);
}

// Write keypoints back to memory. Use of stream for buffering should hopefully ensure that pixel processing routine does not stall.
static void WriteToMem(int* xkey, int* ykey, hls::stream<int>& xkey_stream, hls::stream<int>& ykey_stream, int rows_in, int cols_in, int stride)
{
    int size = rows_in * cols_in; // Assume for now that number of rows is multiple of 4.

    int max_keypoints = MAX_OUT;

    if (size < max_keypoints) max_keypoints = size;

    // Hopefully, sequential writing to x and y arrays with stream for buffering will avoid this causing flow problems in processing
    for (int i = 0; i < max_keypoints; i++) {
        int newx = xkey_stream.read();
        int newy = ykey_stream.read();
        xkey[i] = newx;
        ykey[i] = newy;
        if (newx == -1) break; // Use -1, -1 keypoint to signal the end of the output stream
    }

    // Conversely, if we get too many keypoints, need to flush buffer to avoid overfilling
    int max_flush = size - max_keypoints;
    for (int i = 0; i < max_flush; i++) {
        int newx = xkey_stream.read();
        int newy = ykey_stream.read();
        if (newx == -2) break; // Look for -2, -2 to indicate that routine has finished, in case we have overflow problems and need to flush.
    }
}

extern "C"{
void mp_fast(unsigned char* inputImage, int rows_in, int cols_in, int stride, int* xkey, int* ykey) {
    // Question - are all these variables needed? Outimage and stride not used, and conversely we would like a variable to specify number of keypoints found. Deleted outImage, in case this affects throughput.
    // Don't know if we're using HBM, but processing one pixel per cycle doesn't need this
    #pragma HLS INTERFACE m_axi     port=inputImage  offset=slave bundle=gmem1
    // Split output across two memories
    #pragma HLS INTERFACE m_axi     port=xkey  offset=slave bundle=gmem1
    #pragma HLS INTERFACE m_axi     port=ykey  offset=slave bundle=gmem2
    #pragma HLS INTERFACE s_axilite port=rows_in
    #pragma HLS INTERFACE s_axilite port=cols_in
    #pragma HLS INTERFACE s_axilite port=stride
    #pragma HLS INTERFACE s_axilite port=return

    // Using the "read, process, store" approach recommended in developer guide
    // This should allow fast writing to and from RAM, with streams buffering data as needed
    #pragma HLS DATAFLOW

    int size = rows_in*cols_in;
    int xwidth = cols_in;

	// Stream of pixels from memory to MP-FAST, then stream of keys from MP-FAST to output
    static hls::stream<unsigned char> input_stream("input_stream");
    static hls::stream<int> xkey_stream("xkey_stream");
    static hls::stream<int> ykey_stream("ykey_stream");
    // Have a nice long buffer for input pixels to help data flow
    #pragma HLS STREAM variable = input_stream depth = 128
    #pragma HLS STREAM variable = xkey_stream depth = 32
    #pragma HLS STREAM variable = ykey_stream depth = 32


	// Read image data from global memory over AXI4 MM, and stream pixels out
    ReadFromMem(inputImage, input_stream, rows_in, cols_in, stride);

	// Process incoming stream of pixels, and stream pixels out
	MPFASTprocess(input_stream, rows_in, cols_in, stride, xkey_stream, ykey_stream);

	// Write incoming stream of pixels and write them to global memory
	WriteToMem(xkey, ykey, xkey_stream, ykey_stream, rows_in, cols_in, stride);
}
}
