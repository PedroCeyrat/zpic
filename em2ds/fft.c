#include "fft.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/***************************************************************************************
 1D FFT of complex data
****************************************************************************************/

void butterfly2( float complex * restrict const out, const unsigned int stride,
    const float complex * restrict const phase, const unsigned int m)
{
    unsigned int i;
    float complex t0, t1;

    for( i = 0; i < m; i++) {
        t0 = out[i    ];
        t1 = out[i + m] * phase[i*stride];
        out[i    ] = t0 + t1;
        out[i + m] = t0 - t1;
    }
}


void butterfly3( float complex * restrict const out, const unsigned int stride,
    const float complex * restrict const phase, const unsigned int m)
{
    float complex t0, t1, t2;
    float complex s0, s1;
    unsigned int i;

    const float complex ima = I * cimag(phase[ stride * m ]);

    for( i = 0; i<m; i++) {
        t0 = out[i      ];
        t1 = out[i +   m] * phase[     i * stride ];
        t2 = out[i + 2*m] * phase[ 2 * i * stride ];

        s0 = t1 + t2;
        s1 = t1 - t2;

        out[i      ] = t0 + s0;
        out[i +   m] = t0 - 0.5f * s0 + ima * s1;
        out[i + 2*m] = t0 - 0.5f * s0 - ima * s1;
    }
}

void butterfly4( float complex * restrict const out, const unsigned int stride,
    const float complex * restrict const phase, const unsigned int m,
    const enum fft_direction direction )
{
    unsigned int i;
    float complex t0,t1,t2,t3;
    float complex s0, s1, s2, s3;

    const float complex p = ( direction == FFT_BACKWARD )? I : -I;

    for( i = 0; i < m; i++){
        t0 = out[i      ];
        t1 = out[i +   m] * phase[     i * stride ];
        t2 = out[i + 2*m] * phase[ 2 * i * stride ];
        t3 = out[i + 3*m] * phase[ 3 * i * stride ];

        s0 = t1 + t3;
        s1 = t1 - t3;
        s2 = t0 + t2;
        s3 = t0 - t2;

        out[i      ] = s2 + s0;
        out[i +   m] = s3 + p * s1;
        out[i + 2*m] = s2 - s0;
        out[i + 3*m] = s3 - p * s1;
    }
}

void butterfly5( float complex * restrict const out, const unsigned int stride,
    const float complex * restrict const phase, const unsigned int m)
{
    unsigned int i;
    float complex t0, t1, t2, t3, t4;
    float complex s0, s1, s2, s3, s4, s5, s6, s7;

    const float ra = creal(phase[     stride * m ]);
    const float ia = cimag(phase[     stride * m ]);

    const float rb = creal(phase[ 2 * stride * m ]);
    const float ib = cimag(phase[ 2 * stride * m ]);

    for( i = 0; i<m; i++) {

        t0 = out[       i ];
        t1 = out[   m + i ] * phase[     i * stride ];
        t2 = out[ 2*m + i ] * phase[ 2 * i * stride ];
        t3 = out[ 3*m + i ] * phase[ 3 * i * stride ];
        t4 = out[ 4*m + i ] * phase[ 4 * i * stride ];

        s0 = t1 + t4;
        s1 = t1 - t4;
        s2 = t2 + t3;
        s3 = t2 - t3;

        s4 = t0 + s0 * ra + s2 * rb;
        s5 = t0 + s0 * rb + s2 * ra;

        s6 = -I * (s1 * ia + s3 * ib);
        s7 =  I * (s1 * ib - s3 * ia);

        out[ i       ] = t0 + s0 + s2;
        out[ i +   m ] = s4 - s6;
        out[ i + 2*m ] = s5 + s7;
        out[ i + 3*m ] = s5 - s7;
        out[ i + 4*m ] = s4 + s6;
    }
}


void butterflyN(float complex * restrict const out, const unsigned int stride,
    const float complex * restrict const phase, const unsigned int m, const unsigned int p,
    const unsigned int n)
{

    float complex t[p];

    unsigned int i,j,k;

    for( i = 0; i<m; i++) {

        for( j = 0; j < p; j++) {
            t[j] = out[i + m*j];
        }

        for( j=0; j <p; j++ ) {
            unsigned int tstride = (i + j*m) * stride;
            float complex s = t[0];
            for( k = 1; k<p; k++){
                s += t[k] * phase[ (k*tstride) % n ];
            }
            out[i + j*m] = s;
        }
    }

}


void transform( const float complex in[], const unsigned int in_stride,
                float complex out[],  const unsigned int out_stride,
                t_fft_factors factors[], t_fft_cfg *cfg )

{
    int i;
    int p, m;

    p = factors[0].p;
    m = factors[0].n;

    if ( m == 1 ) {
        for( i = 0; i<p; i++) {
            out[i] = in[ i * in_stride * out_stride ];
        }
    } else {
        for( i = 0; i<p; i++) {
            transform( &in[i * out_stride * in_stride], in_stride,
                       &out[i*m], out_stride * p,
                       &factors[1], cfg);
        }
    }

    switch (p){
        case 2 :
            butterfly2( out, out_stride, cfg -> phase, m ); break;
        case 3 :
            butterfly3( out, out_stride, cfg -> phase, m ); break;
        case 4 :
            butterfly4( out, out_stride, cfg -> phase, m, cfg -> direction ); break;
        case 5 :
            butterfly5( out, out_stride, cfg -> phase, m ); break;
        default :
            butterflyN( out, out_stride, cfg -> phase, m, p, cfg -> n );
    }
}

int fft_init_factors( t_fft_cfg* cfg )
{
    unsigned int i, p;
    unsigned int n = cfg -> n;
    const unsigned int s = floor( sqrt((double) cfg -> n));


    for(i=0, p=4; n>1; i++) {
        if ( i >= MAX_FACTORS ) {
            fprintf(stderr, "(*error*) Unable to factorize fft size.\n");
            return -1;
        }

        while ( n % p ) {
            switch (p) {
                case (4) : p = 2; break;
                case (2) : p = 3; break;
                default  : p += 2;
            }
            if ( p > s ) p = n;
        }

        n = n / p;
        cfg->factors[i].p = p;
        cfg->factors[i].n = n;

        // printf(" %d | %d \n", p, n);
    }

    return 0;
}

int fft_init_cfg( t_fft_cfg* cfg, unsigned int n, enum fft_direction direction ){

    cfg -> n = n;
    cfg -> direction = direction;


    if ( ! ( cfg -> phase = (float complex *) malloc( n * sizeof(float complex)) ) ) {
        fprintf(stderr, "(*error*) Unable to allocate temporary memory for fft\n");
        return -1;
    }

    double phase_mult =  ( direction == FFT_BACKWARD ) ? (2 * M_PI) / n : - (2 * M_PI) / n;
    for( unsigned int i=0; i<n; i++) {
        cfg -> phase[i] = cos( i * phase_mult ) + I * sin( i * phase_mult );
    }

    return fft_init_factors( cfg );
}

int fft_cleanup_cfg( t_fft_cfg* cfg ){
    free( cfg -> phase );
    cfg -> n = 0;

    return 0;
}


void fft_c2c( t_fft_cfg* cfg, const float complex in[], float complex out[] ) {

    transform( in, 1, out, 1, cfg -> factors, cfg );

}

float fft_dk( const unsigned int n, const float dx) {

    float dk = (2 * M_PI) / ( n * dx );

    return dk;
}

/***************************************************************************************
 1D FFT of real data
****************************************************************************************/

int fftr_init_cfg( t_fftr_cfg* rcfg, unsigned int nr, enum fft_direction direction )
{
    unsigned int i, n;

    if ( nr % 2 ) {
        fprintf(stderr, "(*error*) real ffts are implemented for even sized arrays only.\n");
        return -1;
    }

    n = nr / 2;

    fft_init_cfg( &rcfg -> cfg, n, direction);

    if ( ! ( rcfg -> phase = (float complex *) malloc( (n/2) * sizeof(float complex)) ) ) {
        fprintf(stderr, "(*error*) Unable to allocate temporary memory for fft\n");
        return -1;
    }

    const double p =  ( direction == FFT_BACKWARD ) ? M_PI : - M_PI;
    for( i=0; i<n/2; i++) {
        double phase = p * ( (double) (i+1)/n + 0.5 );
        rcfg -> phase[i] = cos( phase ) + I * sin( phase );
    }

    return 0;
}

int fftr_cleanup_cfg( t_fftr_cfg* rcfg ){
    fft_cleanup_cfg( & rcfg -> cfg );
    free( rcfg -> phase );

    return 0;
}

void fftr_r2c( t_fftr_cfg* rcfg, const float in[], float complex out[] )
{
    if ( rcfg -> cfg.direction != FFT_FORWARD ) {
        fprintf(stderr, "(*error*) configuration for fftr_r2c must have direction = FFT_FORWARD\n");
        return;
    }

    const unsigned int n = rcfg -> cfg.n;
    float complex *buffer = (float complex *) malloc( n * sizeof(float complex) );

    fft_c2c( &rcfg -> cfg, (const float complex *) in, buffer );

    // The first (DC) and last (Nyquist frequency) points are purely real
    // and are calculated from the DC points of the complex transform
    out[0] = creal(buffer[0]) + cimag(buffer[0]);
    out[n] = creal(buffer[0]) - cimag(buffer[0]);

    unsigned int i;
    for( i = 1; i <= n/2; i++) {
        float complex z0, z1, s0, s1;

        z0 = buffer[i];
        z1 = conjf( buffer[n-i] );

        s0 = z0 + z1;
        s1 = (z0 - z1) * rcfg -> phase[i-1];

        out[i]   = 0.5f * ( s0 + s1 );
        out[n-i] = 0.5f * conjf( s0 - s1 );
    }

    free( buffer );

}

void fftr_c2r( t_fftr_cfg* rcfg, const float complex in[], float out[] ) {

    if ( rcfg -> cfg.direction != FFT_BACKWARD ) {
        fprintf(stderr, "(*error*) configuration for fftr_c2r must have direction = FFT_BACKWARD\n");
        return;
    }

    const unsigned int n = rcfg -> cfg.n;
    float complex *buffer = (float complex *) malloc( n * sizeof(float complex) );

    buffer[0] = ( creal(in[0]) + creal(in[n]) ) +
            I * ( creal(in[0]) - creal(in[n]) );

    unsigned int i;

    for( i = 1; i <= n/2; i++) {
        float complex z0, z1, s0, s1;
        z0 = in[i];
        z1 = conjf( in[n-i]);

        s0 = z0 + z1;
        s1 = (z0 - z1) * rcfg -> phase[i-1];

        buffer[i  ] =      ( s0 + s1 );
        buffer[n-i] = conjf( s0 - s1 );
    }


    const float norm = 1.0 / (2*n);
    for( i = 0; i < n; i++) buffer[i] *= norm;


    fft_c2c( &rcfg -> cfg, buffer, (float complex *) out);

    free( buffer );
}

/***************************************************************************************
 2D FFT of real data
****************************************************************************************/

/*
nx - grid cell size along x
ny - grid cell size along y

stridey - distance between points along the y direction.

If the input data has "guard cells" along x, the user should set the stridey
parameter to (nx + #guard cells). Otherwise this value can be set to 0 and it
will default to nx.

*/

int fftr2d_init_cfg( t_fftr2d_cfg* cfg, unsigned int nx, unsigned int ny,
    unsigned int stridey, enum fft_direction direction ) {

    int ierr;

    cfg -> nx = nx;
    cfg -> ny = ny;

    if ( stridey > 0 ) {
      cfg -> stridey = stridey;
    } else {
      cfg -> stridey = nx;
    }

    if ((ierr = fftr_init_cfg( &cfg -> cfgx, nx, direction ))) {
      return(ierr);
    }

    if ((ierr = fft_init_cfg( &cfg -> cfgy, ny, direction ))) {
      return(ierr);
    }

    return(0);
}

int fftr2d_cleanup_cfg( t_fftr2d_cfg* cfg ){
  fftr_cleanup_cfg( &cfg->cfgx);
  fft_cleanup_cfg( &cfg->cfgy);
  cfg -> nx = cfg -> ny = 0;

  return(0);
}

/*

The output of a real to complex 2D transform of {nx,ny} data using fftr2d_r2c
is a transposed array of dimensions {ny, nx/2+1}

*/

void fftr2d_r2c( t_fftr2d_cfg* cfg, const float* in, float complex * out ) {
  unsigned int i,j;

  unsigned const r2c_size = cfg -> nx/2 + 1;

  // Do a real to complex tranform for all the lines and Transpose
  // the result
  for( j = 0; j < cfg -> ny; j++) {
    float complex tmp[ r2c_size ];
    fftr_r2c( &cfg->cfgx, &in[ j* cfg->stridey ], tmp);
    for( i = 0; i < r2c_size; i++ ){
      out[ j + i*cfg -> ny ] = tmp[i];
    }
  }

  // Do an "in-place" complex to complex transform of all the lines
  for( j = 0; j < r2c_size; j++ ) {
    float complex tmp[ cfg -> ny ];
    fft_c2c( &cfg->cfgy, &out[ j* cfg->ny ], tmp);
    for( i = 0; i < cfg -> ny; i++ ) {
      out[ i + j* cfg->ny ] = tmp[i];
    }
  }
}

void fftr2d_c2r( t_fftr2d_cfg* cfg, const float complex* in, float * out ) {
  unsigned int i,j;

  unsigned const r2c_size = cfg -> nx/2 + 1;

  // Since the size of out[] is less than the size of in[]
  // we cannot use it for the intermediate storage
  float complex tmp[ cfg -> ny * r2c_size ];

  // Normalization for the y back transform; the x back transform is
  // already normalized
  const float norm = 1.0/cfg -> ny;

  // Do a complex to complex transform of all the lines and transpose the
  // result
  for( j = 0; j < r2c_size; j++) {
    float complex tmp_line[ cfg -> ny ];
    fft_c2c( &cfg->cfgy, &in[ j* cfg -> ny ], tmp_line);
    for( i = 0; i < cfg -> ny; i++ ){
      tmp[j + i*r2c_size ] = tmp_line[i] * norm;
    }
  }

  // Do a complex to real transform of all the lines
  for( j = 0; j < cfg -> ny; j++ ) {
    fftr_c2r( &cfg->cfgx, &tmp[ j* r2c_size ], &out[ j * cfg -> stridey ]);
  }
}
