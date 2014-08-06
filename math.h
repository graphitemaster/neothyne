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

        vec3(float x, float y, float z) :
            x(x),
            y(y),
            z(z)
        { }

        vec3(float a) :
            x(a),
            y(a),
            z(a)
        { }

        float absSquared(void) const {
            return x * x + y * y + z * z;
        }


        float abs(void) const {
            return sqrtf(absSquared());
        }

        void normalize(void) {
            const float length = 1.0f / sqrtf(absSquared());
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
            switch (a) {
                case kAxisX: return xAxis;
                case kAxisY: return yAxis;
                case kAxisZ: return zAxis;
            }
        }

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

        plane(vec3 a, vec3 b, vec3 c) {
            setupPlane(a, b, c);
        }

        plane(vec3 a, vec3 b) {
            setupPlane(a, b);
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

        void setupPlane(const vec3 &p, vec3 nn) {
            n = nn;
            n.normalize();
            d = -n * p;
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

        pointPlane classify(const vec3 &p, float epsilon) const {
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

    ///! quaternion
    struct quat {
        float x;
        float y;
        float z;
        float w;

        quat() {
            *this = identity;
        }

        quat(float x, float y, float z, float w) :
            x(x),
            y(y),
            z(z),
            w(w)
        { }

        quat(const vec3 &axis, float angle) {
            rotationAxis(axis, angle);
        }

        quat(const quat &q1, const quat &q2, float t) {
            slerp(this, q1, q2, t);
        }

        void direction(vec3 *dir, vec3 *up, vec3 *side) const {
            if (side) {
                side->x = 1.0f - 2.0f * (y * y + z * z);
                side->y = 2.0f * (x * y + w * z);
                side->z = 2.0f * (x * z + w * y);
            }

            if (up) {
                up->x = 2.0f * (x * y - w * z);
                up->y = 1.0f - 2.0f * (x * x + z * z);
                up->z = 2.0f * (y * z + w * x);
            }

            if (dir) {
                dir->x = 2.0f * (x * z + w * y);
                dir->y = 2.0f * (y * z - w * x);
                dir->z = 1.0f - 2.0f * (x * x + y * y);
            }
        }

        void rotationAxis(vec3 v, float angle) {
            const float s = sinf(angle / 2);
            const float c = cosf(angle / 2);

            v.normalize();
            x = s * v.x;
            y = s * v.y;
            z = s * v.z;
            w = c;
        }

        void invert(void) {
            x = -x;
            y = -y;
            z = -z;
        }

        quat inverse(void) const {
            quat q;
            q = *this;
            q.invert();
            return q;
        }

        void normalize(void) {
            const float square = x * x + y * y + z * z + w * w;
            // Only normalize if the length isn't already one and we're not a zero.
            if (fabsf(square) > kEpsilon && fabsf(square - 1.0f) > kEpsilon) {
                const float inv = 1.0f / sqrtf(square);
                x *= inv;
                y *= inv;
                z *= inv;
                w *= inv;
            }
        }

        quat normalized(void) const {
            quat q(*this);
            q.normalize();
            return q;
        }

        bool isNormalized(void) const {
            return fabsf(x * x + y * y + z * z + w * w - 1.0f) < kEpsilon;
        }

        // Given a normal quaternion the following function will calculate the
        // value for W when only X, Y and Z are known.
        void calculateW(void) {
            const float t = 1.0f - x * x - y * y - z * z;
            w = t < 0.0f ? 0.0f : -sqrtf(t);
        }

        static void slerp(quat *dest, const quat &q1, const quat &q2, float t);
        static const quat identity;
    };
}
#endif
