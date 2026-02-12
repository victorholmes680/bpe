#include <assert.h>
#include <bits/time.h>
#include <stddef.h>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#define FLAG_IMPLEMENTATION
#include "flag.h"

#include "bpe.h"

double get_secs(void){
    struct timespec tp = {0};
    int ret = clock_gettime(CLOCK_MONOTONIC, &tp);
    assert(ret == 0);
    return (double)tp.tv_sec + (double)tp.tv_nsec*1e-9;
}


bool dump_state(size_t iteration, const char *output_dir_path, Pairs pairs, Tokens tokens) {
    const char *output_file_path = temp_sprintf("%s%zu.bpe", output_dir_path, iteration);
    if(!dump_pairs(output_file_path, pairs)) return false;
    printf("INFO: generated %s\n", output_file_path);

    output_file_path = temp_sprintf("%s%zu.tkn", output_dir_path, iteration);
    if(!dump_tokens(output_file_path, tokens)) return false;
    printf("INFO: generated %s\n", output_file_path);

    return true;
}



// one sample represent time consumed at each iteration
void report_progress(size_t iteration, Tokens tokens_in, Pairs pairs, double *profile_samples, size_t profile_samples_count) {
    double average_profile_samples = 0.0f;
    for(size_t i = 0; i < profile_samples_count; ++i) {
	average_profile_samples += profile_samples[i];
    }

    average_profile_samples /= profile_samples_count;

    printf("INFO: -------    ITERATION:		%zu -------\n", iteration);
    printf("INFO:            Token count:	%zu\n", tokens_in.count);
    printf("INFO:            Pair count:	%zu\n", pairs.count);
    printf("INFO:            Time:		%lfsecs (avg. of %zu iter.)\n", average_profile_samples, profile_samples_count);
    
}

void usage(void) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", flag_program_name());
    fprintf(stderr, "[OPTIONS]:\n");
    flag_print_options(stderr);
}

typedef struct {
    uint32_t l, r;
} Pair_Key;

typedef struct {
    Pair_Key key;
    size_t value;
} Freq;

Freq *collect_freqs(Tokens tokens_in) {
    Freq *freq = NULL;

    for(size_t i = 0; i < tokens_in.count - 1; ++i) { 
	Pair_Key pair = {.l = tokens_in.items[i], .r = tokens_in.items[i+1]};
	ptrdiff_t place = hmgeti(freq, pair);
	if(place < 0) hmput(freq, pair, 1);
	else freq[place].value += 1;
    }
    return freq;
}


int main(int argc, char **argv) {
    // initialize the parameters of command
    uint64_t *report_freq	= flag_uint64("report-freq", 500, "Per how many iterations report the message");
    uint64_t *dump_freq		= flag_uint64("dump-freq", 500, "Per how many iterations dump the state");
    uint64_t *term_freq		= flag_uint64("term-freq", 1, "Termination pair frequence");
    uint64_t *max_iterations	= flag_uint64("max-iterations", 0, "Maximum amount of iterations. 0 means no limit");
    uint64_t *threads_count	= flag_uint64("threads-count", 1, "Thread counts");
    bool *help			= flag_bool("help", false, "Print this help");
    char **input_file		= flag_str("input-file", NULL, "Input text file (MANDATORY)");
    char **output_dir		= flag_str("output-dir", NULL, "Output directory (MANDATORY)");

    // decode the parameter error
    if(!flag_parse(argc, argv)) { // parse the parameter value to the pointer
	usage();
	fprintf(stderr, "ERROR: no %s is provided\n", flag_name(input_file));
	return 1;
    }

    if(*help) {
	usage();
	return 0;
    }


    if(*input_file == NULL) {
	usage();
	fprintf(stderr, "ERROR: no %s is provided\n", flag_name(input_file));
	return 1;
    }

    const char *input_file_path = *input_file; // get the value of input_file

    if(*output_dir == NULL) {
	usage();
	fprintf(stderr, "ERROR: no %s is provided\n", flag_name(output_dir));
	return 1;
    }

    const char *output_dir_path = *output_dir;

    if(*threads_count < 0) {
	*threads_count = 1;
    }


    // RETURNS:
    //  0 - file does not exists
    //  1 - file exists
    int output_dir_exist = file_exists(output_dir_path);

    if(output_dir_exist < 0) return 1;

    // make sure the output dir is clean and not exist before
    if(output_dir_exist) {
	fprintf(stderr, "ERROR: Directory %s already exists, delete it or rename it to not lose any data in it\n",output_dir_path);
	return 1;
    }


    // create new clean output dir
    if(!mkdir_if_not_exists(output_dir_path)) return 1;

    String_Builder sb = {0};

    Freq *freq = NULL;

    Pairs pairs = {0};

    Tokens tokens_in = {0};
    Tokens tokens_out = {0};

    if(!read_entire_file(input_file_path, &sb)) return 1;

    // initialize the pairs (256)
    for(uint32_t i = 0; i < BPE_PRELUDE_SIZE; ++i) {
	da_append(&pairs, ((Pair){.l = i}));
    }

    // construct the tokens_in 
    for(uint32_t i = 0; i < sb.count; ++i) {
	da_append(&tokens_in, (uint8_t)sb.items[i]);
    }

    // profile_samples pointer points to the starting position of a new memeory
    // so we could see profile_samples as an array
    double *profile_samples = malloc((*report_freq)*sizeof(*profile_samples));

    assert(profile_samples != NULL); // make sure the memory has been allocated


    // initial freq from original text string
    freq = collect_freqs(tokens_in);

    size_t iteration = 0;

    // the real core logic
    for(; *max_iterations == 0 || iteration < *max_iterations; ++iteration) {
	if(iteration %(*report_freq) == 0) report_progress(iteration, tokens_in, pairs, profile_samples, *report_freq);
	
	if(iteration %(*dump_freq) == 0) if(!dump_state(iteration, output_dir_path, pairs, tokens_in)) return 1;


	double begin_secs = get_secs();

	// ----------------------------------------------
	ptrdiff_t max_index = 0;
	for(ptrdiff_t i = 1; i < hmlen(freq); ++i){
	    if(freq[i].value > freq[max_index].value || (freq[i].value == freq[max_index].value && memcmp(&freq[i].key,&freq[max_index].key, sizeof(freq[i].key)) > 0)) {
		max_index = i;
	    }
	}
	if(freq[max_index].value <= (*term_freq)) break; // compression is done

	Pair_Key max_pair = freq[max_index].key;

	uint32_t max_token = pairs.count;

	// update the pairs table which conclude the compression path
	da_append(&pairs, ((Pair) {
	    .l		= max_pair.l,
	    .r		= max_pair.r,
	    .freq	= freq[max_index].value,
	}));

	tokens_out.count = 0;


	// execute the compression operation and update the freq table
	for(size_t i = 0; i < tokens_in.count; ) {
	    if(i >= tokens_in.count - 1) {
		da_append(&tokens_out, tokens_in.items[i]);
		i += 1;
	    } else {
		Pair_Key pair;
		pair.l = tokens_in.items[i];
		pair.r = tokens_in.items[i+1];
		if(memcmp(&pair, &max_pair, sizeof(pair)) == 0) {
		    ptrdiff_t place;
		    if(tokens_out.count > 0) {
			pair.l = tokens_out.items[tokens_out.count - 1];

			pair.r = tokens_in.items[i];
			place = hmgeti(freq, pair);
			if(place < 0){
			    printf("%s:%d: i = %zu, pair = (%u, %u)\n", __FILE__, __LINE__, i, pair.l , pair.r);
			    abort();
			}

			assert(freq[place].value > 0);
			freq[place].value -= 1;

			pair.r = max_token;
			place = hmgeti(freq, pair);
			if(place < 0) hmput(freq, pair, 1);
			else freq[place].value += 1;			
		    }

		    pair = max_pair;

		    place = hmgeti(freq, pair);
		    assert(place >= 0);
		    assert(freq[place].value > 0);
		    freq[place].value -= 1;

		    da_append(&tokens_out, max_token);
		    i += 2;

		    // limit the cursor i less than tokens_in count
		    if(i < tokens_in.count){
			pair.r = tokens_in.items[i];

			pair.l = tokens_in.items[i-1];
			place = hmgeti(freq, pair);
			assert(place >= 0);
			assert(freq[place].value > 0);
			freq[place].value -= 1;

			pair.l = max_token;
			place = hmgeti(freq, pair);
			if(place < 0) hmput(freq, pair, 1);
			else freq[place].value += 1;
		    }		    
		} else {
		    da_append(&tokens_out, tokens_in.items[i]);
		    i += 1;
		}
 	    }
	}
	// ----------------------------------------------

	profile_samples[iteration%(*report_freq)] = get_secs() - begin_secs;
	swap(Tokens, tokens_in, tokens_out);	
    }

    size_t remainder_iterations = iteration%(*report_freq);
    report_progress(iteration, tokens_in, pairs, profile_samples, remainder_iterations == 0 ? *report_freq : remainder_iterations);
    
    if(!dump_state(iteration, output_dir_path, pairs, tokens_in)) return 1;

    return 0;
}
