#ifndef MATH_HDR
#define MATH_HDR
#include <math.h>
#include <stddef.h>

namespace m {
    ///! constants
    const float kPi        = 3.141592654f;
    const float kPiHalf    = kPi * 0.5f;
    const float kSqrt2Half = 0.707106781186547f;
    const float kSqrt2     = 1.4142135623730950488f;
    const float kEpsilon   = 0.00001f;
    const float kDegToRad  = kPi / 180.0f;
    const float kRadToDeg  = 180.0f / kPi;

    inline float toRadian(float x) { return x * kDegToRad; }
    inline float toDegree(float x) { return x * kRadToDeg; }

    enum axis {
        kAxisX,
        kAxisY,
        kAxisZ
    };

    ///! vec3
    struct vec3 {
        float x;
        float y;
        float z;

        vec3() :
            x(0.0f),
            y(0.0f),
            z(0.0f)
        { }

        vec3(float nx, float ny, float nz) :
            x(nx),
            y(ny),
            z(nz)
        { }

        vec3(float a) :
            x(a),
            y(a),
            z(a)
        { }

        void rotate(float angle, const vec3 &axe);

        float absSquared(void) const {
            return x * x + y * y + z * z;
        }

        float abs(void) const {
            return sqrtf(x * x + y * y + z * z);
        }

        void normalize(void) {
            const float length = 1.0f / abs();
            x *= length;
            y *= length;
            z *= length;
        }

        vec3 normalized(void) const {
            const float scale = 1.0f / abs();
            return vec3(x * scale, y * scale, z * scale);
        }

        bool isNormalized(void) const {
            return fabsf(abs() - 1.0f) < kEpsilon;
        }

        bool isNull() const {
            return x == 0.0f && y == 0.0f && z == 0.0f;
        }

        bool isNullEpsilon(const float epsilon = kEpsilon) const {
            return equals(vec3(0.0f, 0.0f, 0.0f), epsilon);
        }

        bool equals(const vec3 &cmp, const float epsilon) const {
            return (fabsf(x - cmp.x) < epsilon)
                && (fabsf(y - cmp.y) < epsilon)
                && (fabsf(z - cmp.z) < epsilon);
        }

        vec3 cross(const vec3 &v) const {
            return vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
        }

        vec3 &operator +=(const vec3 &vec) {
            x += vec.x;
            y += vec.y;
            z += vec.z;
            return *this;
        }

        vec3 &operator -=(const vec3 &vec) {
            x -= vec.x;
            y -= vec.y;
            z -= vec.z;
            return *this;
        }

        vec3 &operator *=(float value) {
            x *= value;
            y *= value;
            z *= value;
            return *this;
        }

        vec3 &operator /=(float value) {
            const float inv = 1.0f / value;
            x *= inv;
            y *= inv;
            z *= inv;
            return *this;
        }

        vec3 operator -(void) const {
            return vec3(-x, -y, -z);
        }

        float operator[](size_t index) const {
            return (&x)[index];
        }

        float &operator[](size_t index) {
            return (&x)[index];
        }

        static inline const vec3 getAxis(axis a) {
            if (a == kAxisX) return xAxis;
            else if (a == kAxisY) return yAxis;
            return zAxis;
        }

        static bool rayCylinderIntersect(const vec3 &start, const vec3 &direction,
            const vec3 &edgeStart, const vec3 &edgeEnd, float radius, float *fraction);

        static bool raySphereIntersect(const vec3 &start, const vec3 &direction,
            const vec3 &sphere, float radius, float *fraction);

        static const vec3 xAxis;
        static const vec3 yAxis;
        static const vec3 zAxis;
    };

    // global operators
    inline vec3 operator+(const vec3 &a, const vec3 &b) {
        return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    inline vec3 operator-(const vec3 &a, const vec3 &b) {
        return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    inline vec3 operator*(const vec3 &a, float value) {
        return vec3(a.x * value, a.y * value, a.z * value);
    }

    inline vec3 operator*(float value, const vec3 &a) {
        return vec3(a.x * value, a.y * value, a.z * value);
    }

    inline vec3 operator/(const vec3 &a, float value) {
        const float inv = 1.0f / value;
        return vec3(a.x * inv, a.y * inv, a.z * inv);
    }

    // cross product
    inline vec3 operator^(const vec3 &a, const vec3 &b) {
        return vec3((a.y * b.z - a.z * b.y),
                    (a.z * b.x - a.x * b.z),
                    (a.x * b.y - a.y * b.x));
    }

    // dot product
    inline float operator*(const vec3 &a, const vec3 &b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // equality operators
    inline bool operator==(const vec3 &a, const vec3 &b) {
        return (fabs(a.x - b.x) < kEpsilon)
            && (fabs(a.y - b.y) < kEpsilon)
            && (fabs(a.z - b.z) < kEpsilon);
    }

    inline bool operator!=(const vec3 &a, const vec3 &b) {
        return (fabsf(a.x - b.x) > kEpsilon)
            || (fabsf(a.y - b.y) > kEpsilon)
            || (fabsf(a.z - b.z) > kEpsilon);
    }

    ///! plane
    enum pointPlane {
        kPointPlaneBack,
        kPointPlaneOnPlane,
        kPointPlaneFront
    };

    struct plane {
        plane(void) :
            d(0.0f)
        { }

        plane(vec3 p1, vec3 p2, vec3 p3) {
            setupPlane(p1, p2, p3);
        }

        plane(vec3 pp, vec3 nn) {
            setupPlane(pp, nn);
        }

        plane(float a, float b, float c, float d) {
            setupPlane(a, b, c, d);
        }

        plane(const vec3 &nn, float dd) :
            n(nn),
            d(dd)
        { }

        void setupPlane(const vec3 &p1, const vec3 &p2, const vec3 &p3) {
            n = (p2 - p1) ^ (p3 - p1);
            n.normalize();
            d = -n * p1;
        }

        void setupPlane(const vec3 &pp, vec3 nn) {
            n = nn;
            n.normalize();
            d = -n * pp;
        }

        void setupPlane(vec3 nn, float dd) {
            n = nn;
            d = dd;
        }

        void setupPlane(float a, float b, float c, float d) {
            n = vec3(a, b, c);
            d = d / n.abs();
            n.normalize();
        }

        vec3 project(const vec3 &point) const {
            float distance = getDistanceFromPlane(point);
            return vec3(point.x - n.x * distance,
                        point.y - n.y * distance,
                        point.z - n.z * distance);
        }

        bool getIntersection(float *f, const vec3 &p, const vec3 &v) const {
            const float q = n * v;
            // Plane and line are parallel?
            if (fabsf(q) < kEpsilon)
                return false;

            *f = -(n * p + d) / q;
            return true;
        }

        float getDistanceFromPlane(const vec3 &p) const {
            return p * n + d;
        }

        pointPlane classify(const vec3 &p, float epsilon = kEpsilon) const {
            const float distance = getDistanceFromPlane(p);
            if (distance > epsilon)
                return kPointPlaneFront;
            else if (distance < -epsilon)
                return kPointPlaneBack;
            return kPointPlaneOnPlane;
        }

        bool isPlaneBetween(const vec3 &a, const vec3 &b) const {
            const float q = n * (b - a);
            if (fabsf(q) < kEpsilon)
                return true;

            const float t = -(n * a + d) / q;
            return (t > -kEpsilon && t < 1.0f + kEpsilon);
        }

        vec3 n;
        float d;
    };

    ///! quat
    struct quat {
        float x, y, z, w;
        quat() :
            x(0.0f),
            y(0.0f),
            z(0.0f),
            w(1.0f) { }

        quat(float x, float y, float z, float w) :
            x(x),
            y(y),
            z(z),
            w(w) { }

        quat conjugate() {
            return quat(-x, -y, -z, w);
        }
    };

    inline quat operator*(const quat &l, const quat &r) {
        const float w = (l.w * r.w) - (l.x * r.x) - (l.y * r.y) - (l.z * r.z);
        const float x = (l.x * r.w) + (l.w * r.x) + (l.y * r.z) - (l.z * r.y);
        const float y = (l.y * r.w) + (l.w * r.y) + (l.z * r.x) - (l.x * r.z);
        const float z = (l.z * r.w) + (l.w * r.z) + (l.x * r.y) - (l.y * r.x);
        return quat(x, y, z, w);
    }

    inline quat operator*(const quat &q, const vec3 &v) {
        const float w = - (q.x * v.x) - (q.y * v.y) - (q.z * v.z);
        const float x =   (q.w * v.x) + (q.y * v.z) - (q.z * v.y);
        const float y =   (q.w * v.y) + (q.z * v.x) - (q.x * v.z);
        const float z =   (q.w * v.z) + (q.x * v.y) - (q.y * v.x);
        return quat(x, y, z, w);
    }

    ///!mat4
    struct mat4 {
        float m[4][4];
        mat4() { }

        void loadIdentity(void) {
            m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = 0.0f;
            m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = 0.0f;
            m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = 0.0f;
            m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
        }

        mat4 operator*(const mat4 &t) const {
            mat4 r;
            for (size_t i = 0; i < 4; i++)
                for (size_t j = 0; j < 4; j++)
                    r.m[i][j] = m[i][0] * t.m[0][j] + m[i][1] * t.m[1][j] + m[i][2] * t.m[2][j] + m[i][3] * t.m[3][j];
            return r;
        }

        void setScaleTrans(float scaleX, float scaleY, float scaleZ) {
            m[0][0] = scaleX; m[0][1] = 0.0f;   m[0][2] = 0.0f;   m[0][3] = 0.0f;
            m[1][0] = 0.0f;   m[1][1] = scaleY; m[1][2] = 0.0f;   m[1][3] = 0.0f;
            m[2][0] = 0.0f;   m[2][1] = 0.0f;   m[2][2] = scaleZ; m[2][3] = 0.0f;
            m[3][0] = 0.0f;   m[3][1] = 0.0f;   m[3][2] = 0.0f;   m[3][3] = 1.0f;
        }

        void setRotateTrans(float rotateX, float rotateY, float rotateZ) {
            mat4 rx, ry, rz;

            const float x = toRadian(rotateX);
            const float y = toRadian(rotateY);
            const float z = toRadian(rotateZ);

            rx.m[0][0] = 1.0f;    rx.m[0][1] = 0.0f;     rx.m[0][2] = 0.0f;     rx.m[0][3] = 0.0f;
            rx.m[1][0] = 0.0f;    rx.m[1][1] = cosf(x);  rx.m[1][2] = -sinf(x); rx.m[1][3] = 0.0f;
            rx.m[2][0] = 0.0f;    rx.m[2][1] = sinf(x);  rx.m[2][2] = cosf(x);  rx.m[2][3] = 0.0f;
            rx.m[3][0] = 0.0f;    rx.m[3][1] = 0.0f;     rx.m[3][2] = 0.0f;     rx.m[3][3] = 1.0f;
            ry.m[0][0] = cosf(y); ry.m[0][1] = 0.0f;     ry.m[0][2] = -sinf(y); ry.m[0][3] = 0.0f;
            ry.m[1][0] = 0.0f;    ry.m[1][1] = 1.0f;     ry.m[1][2] = 0.0f;     ry.m[1][3] = 0.0f;
            ry.m[2][0] = sinf(y); ry.m[2][1] = 0.0f;     ry.m[2][2] = cosf(y);  ry.m[2][3] = 0.0f;
            ry.m[3][0] = 0.0f;    ry.m[3][1] = 0.0f;     ry.m[3][2] = 0.0f;     ry.m[3][3] = 1.0f;
            rz.m[0][0] = cosf(z); rz.m[0][1] = -sinf(z); rz.m[0][2] = 0.0f;     rz.m[0][3] = 0.0f;
            rz.m[1][0] = sinf(z); rz.m[1][1] = cosf(z);  rz.m[1][2] = 0.0f;     rz.m[1][3] = 0.0f;
            rz.m[2][0] = 0.0f;    rz.m[2][1] = 0.0f;     rz.m[2][2] = 1.0f;     rz.m[2][3] = 0.0f;
            rz.m[3][0] = 0.0f;    rz.m[3][1] = 0.0f;     rz.m[3][2] = 0.0f;     rz.m[3][3] = 1.0f;

            *this = rz * ry * rx;
        }

        void setTranslateTrans(float x, float y, float z) {
            m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f; m[0][3] = x;
            m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f; m[1][3] = y;
            m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f; m[2][3] = z;
            m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
        }

        void setCameraTrans(const vec3 &target, const vec3 &up) {
            vec3 N = target;
            N.normalize();
            vec3 U = up;
            U.normalize();
            U = U.cross(N);
            vec3 V = N.cross(U);
            m[0][0] = U.x;  m[0][1] = U.y;  m[0][2] = U.z;  m[0][3] = 0.0f;
            m[1][0] = V.x;  m[1][1] = V.y;  m[1][2] = V.z;  m[1][3] = 0.0f;
            m[2][0] = N.x;  m[2][1] = N.y;  m[2][2] = N.z;  m[2][3] = 0.0f;
            m[3][0] = 0.0f; m[3][1] = 0.0f; m[3][2] = 0.0f; m[3][3] = 1.0f;
        }

        void setPersProjTrans(float fov, float width, float height, float near, float far) {
            const float ar = width / height;
            const float range = near - far;
            const float halfFov = tanf(m::toRadian(fov / 2.0f));
            m[0][0] = 1.0f/(halfFov * ar);  m[0][1] = 0.0f;         m[0][2] = 0.0f;               m[0][3] = 0.0;
            m[1][0] = 0.0f;                 m[1][1] = 1.0f/halfFov; m[1][2] = 0.0f;               m[1][3] = 0.0;
            m[2][0] = 0.0f;                 m[2][1] = 0.0f;         m[2][2] = (-near -far)/range; m[2][3] = 2.0f * far*near/range;
            m[3][0] = 0.0f;                 m[3][1] = 0.0f;         m[3][2] = 1.0f;               m[3][3] = 0.0;
        }
    };
}
#endif
