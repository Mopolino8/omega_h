#ifndef QR_H
#define QR_H

#define MAX_PTS 16

void qr_decomp(double a[3][3], double q[3][3], double r[3][3]);
void qr_decomp2(
    double a[MAX_PTS][4],
    double q[MAX_PTS][MAX_PTS],
    double r[MAX_PTS][4],
    unsigned npts);
void qr_eigen(double a[3][3], double q[3][3], double l[3][3]);

#endif
