#include <stdio.h>

#include <rsf.h>

int main(int argc, char *argv[])
{
    bool verbose;
    char ahead[SF_EBCBYTES], bhead[SF_BNYBYTES];
    char *headname, *filename, *trace;
    sf_file in, hdr;
    int format, ns, nk, nsegy, itr, ntr, *itrace;
    FILE *head, *file;
    float *ftrace;

    sf_init(argc, argv);

    if (!sf_getbool("verbose",&verbose)) verbose=false;

    sf_endian();

    if (NULL == (filename = sf_getstring("tape")))
	sf_error("Need to specify tape=");

    if (NULL == (file = fopen(filename,"wb")))
	sf_error("Cannot open \"%s\" for writing:",filename);

    if (NULL == (headname = sf_getstring("hfile"))) headname = "header";

    if (NULL == (head = fopen(headname,"r")))
	sf_error("Cannot open file \"%s\" for reading ascii header:",headname);

    if (SF_EBCBYTES != fread(ahead, 1, SF_EBCBYTES, head)) 
	sf_error("Error reading ascii header");
    fclose (head);
  
    if (verbose) sf_warning("ASCII header read from \"%s\"",headname);
  
    sf_asc2ebc (SF_EBCBYTES, ahead);   

    if (SF_EBCBYTES != fwrite(ahead, 1, SF_EBCBYTES, file)) 
	sf_error("Error writing ebcdic header");
 
      if (NULL == (headname = sf_getstring("bfile"))) headname = "binary";

    if (NULL == (head = fopen(headname,"rb")))
	sf_error("Cannot open file \"%s\" for reading binary header:",headname);
    
    if (SF_BNYBYTES != fread(bhead, 1, SF_BNYBYTES, head)) 
	sf_error("Error reading binary header");
    fclose (head);
    
    if (verbose) sf_warning("Binary header read from \"%s\"",headname);

    if (SF_BNYBYTES != fwrite(bhead, 1, SF_BNYBYTES, file))
	sf_error("Error writing binary header");

    format = sf_segyformat (bhead);

    switch (format) {
	case 1:
	    if (verbose) sf_warning("Assuming IBM floating point format");
	    break;
	case 2:
	    if (verbose) sf_warning("Assuming 4 byte integer format");
	    break;
	case 3:
	    if (verbose) sf_warning("Assuming 2 byte integer format");
	    break;
	default:
	    sf_error("Nonstandard format: %d",format);
	    break;
    }

    in = sf_input("in");

    if (!sf_histint(in,"n1",&ns)) ns = sf_segyns (bhead); 

    if (verbose) sf_warning("Detected trace length of %d",ns);

    nsegy = SF_HDRBYTES + ((3 == format)? ns*2: ns*4);    
 
    ntr = sf_leftsize(in,1);

    if (verbose) sf_warning("Expect %d traces",ntr);

    hdr = sf_input("tfile");
    if (!sf_histint(hdr,"n1",&nk) || (SF_NKEYS != nk))
	sf_error ("Need n1=%d keys in tfile",SF_NKEYS);
    if (SF_NKEYS*ntr != sf_filesize(hdr))
	sf_error ("Wrong number of traces in tfile");
    
    trace = sf_charalloc (nsegy);
    ftrace = sf_floatalloc (ns);
    itrace = sf_intalloc (SF_NKEYS); 

    for (itr=0; itr < ntr; itr++) {
	sf_read(itrace,sizeof(int),SF_NKEYS,hdr);	
	sf_head2segy(trace, itrace, SF_NKEYS);

	sf_read (ftrace,sizeof(float),ns,in);
	sf_trace2segy(trace + SF_HDRBYTES, ftrace, ns,format);

	if (nsegy != fwrite(trace, 1, nsegy, file))
	    sf_error ("Error writing trace %d",itr+1);
    }

    exit (0);
}

