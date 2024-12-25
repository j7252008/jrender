#include <iostream>
#include <vector>
#include <type_traits>
#include <memory>
#include <omp.h>
#include <map>

#include <fmt/format.h>

#include "render.hpp"

class A
{
public:
    A() {}

    template <class T>
    void setValue(const std::string& key, T&& v)
    {
        value[key] = std::move(v);
    }

    using ValueType = std::variant<int, double, glm::vec2, glm::vec3, glm::vec4, glm::mat3, glm::mat4>;
    std::map<std::string, ValueType> value;
};

// 计算由三个点构成的三角形的面积
float triangleArea(const glm::dvec2& A, const glm::dvec2& B, const glm::dvec2& C)
{
    return 0.5f * std::abs(A.x * (B.y - C.y) + B.x * (C.y - A.y) + C.x * (A.y - B.y));
}

void calculateBarycentric(const glm::dvec2& P, const glm::dvec2& P1, const glm::dvec2& P2, const glm::dvec2& P3,
                          double& u, double& v, double& w)
{
    // 计算总面积
    float A = triangleArea(P1, P2, P3);

    // 计算与顶点相关的面积
    float A1 = triangleArea(P, P2, P3);
    float A2 = triangleArea(P1, P, P3);
    float A3 = triangleArea(P1, P2, P);

    // 计算重心坐标
    u = A1 / A;
    v = A2 / A;
    w = A3 / A;
}

glm::dvec3 barycentricInterpolation(const glm::dvec3& C1, const glm::dvec3& C2, const glm::dvec3& C3, double u,
                                    double v, double w)
{
    double totalWeight = u + v + w;
    return (u * C1 + v * C2 + w * C3) / totalWeight;
}

glm::dvec3 barycentric_glm(const glm::dvec2 tri[3], const glm::dvec2& P)
{
    glm::mat3 ABC = { glm::dvec3(tri[0], 1.0), glm::dvec3(tri[1], 1.0), glm::dvec3(tri[2], 1.0) };

    // for a degenerate triangle generate negative coordinates, it will be thrown away by the rasterizator
    if (glm::determinant(ABC) < 1e-3) return { -1, 1, 1 };

    return glm::inverse(ABC) * glm::dvec3(P, 1.0);
}

void omp_test()
{
    int sum = 0;
#pragma omp parallel for
    for (size_t i = 0; i < 20; i++) {
        sum += i;
        std::printf("%d ", sum);
    }
}

int main()
{
    A a;
    a.value["a"] = glm::vec3(1);
    a.setValue("a", glm::vec3(2));
    glm::vec3 aa = std::get<glm::vec3>(a.value["a"]);

    glm::dvec2 tri[3] = { glm::dvec2{ 405.882043, 589.993949 }, glm::dvec2{ 413.750075, 590.418093 },
                          glm::dvec2{ 411.557661, 594.802782 } };
    glm::dvec2 p = { 406, 590 };
    glm::dvec3 bary = barycentric_glm(tri, p);
    fmt::print("{} {} {} \n", bary.x, bary.y, bary.z);

    double u, v, w;
    calculateBarycentric(p, tri[0], tri[1], tri[2], u, v, w);
    fmt::print("{} {} {} \n", u, v, w);

    return 0;
}
