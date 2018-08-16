#include <stdio.h>
#include <string.h>
#include <math.h>
#include "portaudio.h"

#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <iostream>
using namespace std;

#define NUM_SECONDS   (5)
#define SAMPLE_RATE   (44100)
#define FRAMES_PER_BUFFER  (64)

#ifndef M_PI
#define M_PI  (3.14159265)
#endif

#define TABLE_SIZE   (1024)

struct paTestData {
    float sine[TABLE_SIZE];
    int left_phase;
    int right_phase;
    char message[20];
    int phase_incr;
};

class AudioCallback {
public:
    
    static int patestCallback( const void *inputBuffer, void *outputBuffer,
                              unsigned long framesPerBuffer,
                              const PaStreamCallbackTimeInfo* timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void *userData )
    {
        paTestData *data = (paTestData*)userData;
        float *out = (float*)outputBuffer;
        unsigned long i;
        
        (void) timeInfo; /* Prevent unused variable warnings. */
        (void) statusFlags;
        (void) inputBuffer;
        
        for (i=0; i<framesPerBuffer; i++)
        {
            *out++ = data->sine[data->left_phase];  /* left */
            *out++ = data->sine[data->right_phase];  /* right */
            data->left_phase += data->phase_incr;
            if (data->left_phase >= TABLE_SIZE)
                data->left_phase -= TABLE_SIZE;
            data->right_phase += data->phase_incr;
            if (data->right_phase >= TABLE_SIZE)
                data->right_phase -= TABLE_SIZE;
        }
        return paContinue;
    }

private:
    
};

class AudioData {
public:
    AudioData () {
        memset(&data.sine, 0, TABLE_SIZE*sizeof(float));
        
        float freq = 1.f;
        int i;
        for( i=0; i<TABLE_SIZE; i++ ) {
            float minus = 1;
            for (int k = 1; k < 40; k+=2) {
                data.sine[i] +=  0.8 * minus * (float)(1./(k*k))*(float) sin( ((double)i/(double)TABLE_SIZE) * M_PI * 2. * k *freq);
                minus *= -1.f;
            }
        }
        
        data.left_phase = data.right_phase = 0;
        data.phase_incr = 1;
    }
    
    paTestData* GetData () {
        return &data;
    }
    
    void SetIncr (int i) { data.phase_incr  = i; }
private:
    paTestData data;
    
};

class AudioStream {
public:
    AudioStream () {
        printf("PortAudio Test: output square wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);
        
        err = Pa_Initialize ();
        if (err != paNoError) {
            Pa_Terminate ();
            fprintf (stderr, "An error occured while using the portaudio stream\n");
            fprintf (stderr, "Error number: %d\n", err );
            fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (err));
            return;
        }
        
        outputParameters.device = Pa_GetDefaultOutputDevice (); /* default output device */
        if (outputParameters.device == paNoDevice) {
            fprintf (stderr,"Error: No default output device.\n");
            Pa_Terminate ();
            return;
        }
        outputParameters.channelCount = 2;       /* stereo output */
        outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
        outputParameters.suggestedLatency = Pa_GetDeviceInfo (outputParameters.device)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = NULL;
    }
    
    ~AudioStream () { Pa_Terminate(); }
    
    PaError Open (AudioCallback* acb, AudioData* adata) {
        err = Pa_OpenStream(
                            &stream,
                            NULL, /* no input */
                            &outputParameters,
                            SAMPLE_RATE,
                            FRAMES_PER_BUFFER,
                            paClipOff,      /* we won't output out of range samples so don't bother clipping them */
                            acb->patestCallback,
                            adata->GetData () );
        
        return err;
    }
    
    PaError Start () {
        return Pa_StartStream (stream);
    }
    
    PaError Stop () {
        return Pa_StopStream (stream);
    }
    
    PaError Close () {
        return Pa_CloseStream (stream);
    }
    
    PaStream* GetStream () { return stream; }
    
private:
    PaStreamParameters outputParameters;
    PaError err;
    PaStream* stream;
};

class Application
{
public:
    Application()
    {
        PaError err = ast.Open (&acb, &adata);
        if (err != paNoError) {
            fprintf (stderr, "Error number: %d\n", err );
            fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (err));
        }
        input = "";
        output = ".";
	cout << "This is audio_yarn." << endl << "Type 's <return>' to start," << endl;
	cout << "type 'q <return>' to quit," << endl;
	cout << "play a melody by typing in integers, for ex. '2  <return>', or '3  <return>', to play different pitches," <<endl;
	
    }
    
    void worker () {
        
        while (true) {
            unique_lock<mutex> mlock (m_mutex);
            while (m_condVar.wait_for (mlock, chrono::milliseconds(100)) == cv_status::timeout) {
                //cout << output;
            }
            int comd = atoi (input.c_str());
            if (comd > 0) {
                    if (Pa_IsStreamActive (ast.GetStream()) == 1)
                        adata.SetIncr (comd); // play with harmonic frequencies
            }
            if (input == "s") { // start
                if (Pa_IsStreamActive (ast.GetStream()) != 1) {
                    PaError err = ast.Start ();
                    if (err != paNoError) {
                        fprintf (stderr, "Error number: %d\n", err );
                        fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (err));
                    }
                }
            }
            if (input == "q") { // quit
                if (Pa_IsStreamActive (ast.GetStream ()) == 1) {
                    PaError err = ast.Stop ();
                    if (err != paNoError) {
                        fprintf (stderr, "Error number: %d\n", err );
                        fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (err));
                    }
                    err = ast.Close();
                    if (err != paNoError) {
                        fprintf (stderr, "Error number: %d\n", err );
                        fprintf (stderr, "Error message: %s\n", Pa_GetErrorText (err));
                    }
                }
                break;
            }
            //output = input;
            //cout << output;
        }
    }
    
    void listener () {
        while (true) {
            cin >> input;
            lock_guard<mutex> guard(m_mutex);
            // Notify the condition variable
            m_condVar.notify_one();
            if (input == "q") // quit message
                break;
        }
    }
private:
    mutex m_mutex;
    condition_variable m_condVar;
    string input;
    string output;

    AudioData adata;
    AudioCallback acb;
    AudioStream ast;
};

int main()
{
    Application app;
    
    thread thread_1 (&Application::listener, &app);
    thread thread_2 (&Application::worker, &app);
    
    thread_2.join ();
    thread_1.join ();
    
    return 0;
}
