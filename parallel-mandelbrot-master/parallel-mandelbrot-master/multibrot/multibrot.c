#include <mpi.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <complex.h>

#define uchar unsigned char

#define X 1920
#define Y 1080

#define R_MAX 1.5
#define R_MIN -2
#define I_MAX 1.0
#define I_MIN -I_MAX

#define MAX_ITER 8000
#define MIN_POWER 1
#define MAX_POWER 10
#define dP 0.01

typedef struct {
    uchar r;
    uchar g;
    uchar b;
} Color;

static inline double lerp(double v0, double v1, double t) {
    return (1 - t) * v0 + t * v1;
}

static inline double max(double a, double b){
    return a > b ? a : b;
}

Color* make_palette(int size);


Color mandelbrot(int px, int py, Color* palette, double power){
    double x0 = R_MIN + (px * ((R_MAX - R_MIN)/(X*1.0))); // complex scale of Px
    double y0 = I_MIN + (py * ((I_MAX - I_MIN)/(Y*1.0))); // complex scale of Py

    double i = 0;
    float complex z = CMPLX(0.0, 0.0);
    float complex c = CMPLX(x0, y0);
    
    while(cabsf(z) <= 20 && i < MAX_ITER){
        z = cpowf(z, power) + c;
        i++;
    }
    

    if(i < MAX_ITER){
        double log_zn = log(cabsf(z)) / power;
        double nu = log(log_zn / log(2.0))/log(max(2.0, abs(power)));
        i += 1.0 - nu;
    }
    Color c1 = palette[(int)i];
    Color c2;
    if((int)i + 1 > MAX_ITER){
        c2 = palette[(int)i];
    }else{
        c2 = palette[((int)i)+1];
    }

    double mod = i - ((int)i) ; // cant mod doubles
    return (Color){
            .r = (int)lerp(c1.r, c2.r, mod),
            .g = (int)lerp(c1.g, c2.g, mod),
            .b = (int)lerp(c1.b, c2.b, mod),
    };

}

void master(int workers, Color* palette, double power){
    MPI_Status status;
    
    for(int i=1;i<workers;i++){
        MPI_Request req; // non-blocking send for faster broadcast
        MPI_Isend(&power, 1, MPI_DOUBLE, i, 1, MPI_COMM_WORLD, &req);
        MPI_Request_free(&req);
    }
    
    uchar (*colors)[X][3] = malloc(sizeof(uchar[Y][X][3]));
    

    int size = Y/(workers-1);
    Color* recv = (Color*)malloc(sizeof(Color)*size*X);
    for(int i=0;i<(workers-1);i++){
        MPI_Recv(recv, sizeof(Color)*size*X, MPI_CHAR, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
        int source = status.MPI_SOURCE - 1;
        for(int x =0;x<size;x++){
            for(int y = 0;y<X;y++){
                Color c = recv[y*size+x];
                int i = source*size + x;
                colors[i][y][0] = c.r;
                colors[i][y][1] = c.g;
                colors[i][y][2] = c.b;
            }
        }
    }
    printf("fin calc\n");

    FILE* fout;
    char buf[20];
    snprintf(buf, sizeof(buf), "output/%.04d.ppm", (int)((power-MIN_POWER)/dP));
    fout = fopen(buf, "w");
    fprintf(fout, "P6\n%d %d\n255\n", X, Y);
    for(int y = 0; y < Y; y++){
        for(int x = 0; x < X; x++){
            fwrite(colors[y][x], 1, 3, fout);
        }
    }
    printf("Finished %.04d\n", (int)((power-MIN_POWER)/dP));
    fclose(fout);
}

void slave(int workers, int rank, Color* palette){
    double power;
    MPI_Recv(&power, 1, MPI_DOUBLE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int size = Y / (workers-1);
    int ssize = sizeof(Color) * size * X;
    Color* buf = (Color*)malloc(ssize);
    for(int y=0;y<size;y++){
        for(int x=0;x<X;x++){
            int j = x * size + y;
            buf[x*size + y] = mandelbrot(x, ((rank-1)*size) + y, palette, power);
        }
    }
    MPI_Send(buf, ssize, MPI_CHAR, 0, 1, MPI_COMM_WORLD);
    free(buf);
}


int main(int argc, char* argv[]){

    int size, rank;

    MPI_Init(&argc, &argv);
    MPI_Comm_size( MPI_COMM_WORLD, &size);
    MPI_Comm_rank( MPI_COMM_WORLD, &rank);
    
    Color* palette = make_palette(MAX_ITER);
    
    for(double i=MIN_POWER; i<=MAX_POWER; i+=dP){
        if(rank == 0){
            master(size, palette, i);
        }else{
            slave(size, rank, palette);
        }
    }

    MPI_Finalize();
    return 0;
}


Color* make_palette(int size){
    Color* palette = (Color*)malloc(sizeof(Color)*(size+1));
        for(int i=0;i<size+1;i++){
        if (i >= size){
            palette[i] = (Color){.r=0,.g=0,.b=0};
            continue;
        }
        double j;
        if(i == 0){
            j = 3.0;
        }else{
            j = 3.0 * (log(i)/log(size-1.0));
        }

        if (j<1){
            palette[i] = (Color){
                    .r = 0,
                    .g = 255 * j,
                    .b = 0
            };
        }else if(j<2){
            palette[i] = (Color){
                    .r = 255*(j-1),
                    .g = 255,
                    .b = 0,
            };
        }else{
            palette[i] = (Color){
                    .r = 255 * (j-2),
                    .g = 255,
                    .b = 255,
            };
        }
    }
    return palette;
}
