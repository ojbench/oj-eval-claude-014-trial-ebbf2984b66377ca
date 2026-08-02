#pragma once
#ifndef BIG_INT_H
#define BIG_INT_H

#include <string>
#include <vector>
#include <algorithm>

class BigInt {
private:
    std::vector<int> digits;
    bool negative;

    void removeLeadingZeros() {
        while (digits.size() > 1 && digits.back() == 0) {
            digits.pop_back();
        }
        if (digits.size() == 1 && digits[0] == 0) {
            negative = false;
        }
    }

public:
    BigInt() : digits({0}), negative(false) {}

    BigInt(long long num) {
        if (num == 0) {
            digits = {0};
            negative = false;
            return;
        }
        negative = num < 0;
        num = std::abs(num);
        while (num > 0) {
            digits.push_back(num % 10);
            num /= 10;
        }
    }

    BigInt(const std::string& s) {
        if (s.empty() || s == "0") {
            digits = {0};
            negative = false;
            return;
        }
        size_t start = 0;
        negative = (s[0] == '-');
        if (negative || s[0] == '+') start = 1;

        for (size_t i = s.length(); i > start; --i) {
            digits.push_back(s[i-1] - '0');
        }
        removeLeadingZeros();
    }

    bool isNegative() const { return negative; }
    bool isZero() const { return digits.size() == 1 && digits[0] == 0; }

    std::string toString() const {
        if (isZero()) return "0";
        std::string result;
        if (negative) result += '-';
        for (int i = digits.size() - 1; i >= 0; --i) {
            result += char('0' + digits[i]);
        }
        return result;
    }

    BigInt abs() const {
        BigInt result = *this;
        result.negative = false;
        return result;
    }

    int compareAbs(const BigInt& other) const {
        if (digits.size() != other.digits.size()) {
            return digits.size() > other.digits.size() ? 1 : -1;
        }
        for (int i = digits.size() - 1; i >= 0; --i) {
            if (digits[i] != other.digits[i]) {
                return digits[i] > other.digits[i] ? 1 : -1;
            }
        }
        return 0;
    }

    bool operator<(const BigInt& other) const {
        if (negative != other.negative) return negative;
        int cmp = compareAbs(other);
        return negative ? cmp > 0 : cmp < 0;
    }

    bool operator>(const BigInt& other) const {
        return other < *this;
    }

    bool operator<=(const BigInt& other) const {
        return !(other < *this);
    }

    bool operator>=(const BigInt& other) const {
        return !(*this < other);
    }

    bool operator==(const BigInt& other) const {
        return negative == other.negative && digits == other.digits;
    }

    bool operator!=(const BigInt& other) const {
        return !(*this == other);
    }

    BigInt operator+(const BigInt& other) const {
        if (negative == other.negative) {
            BigInt result;
            result.negative = negative;
            result.digits.clear();
            int carry = 0;
            size_t maxSize = std::max(digits.size(), other.digits.size());
            for (size_t i = 0; i < maxSize || carry; ++i) {
                int sum = carry;
                if (i < digits.size()) sum += digits[i];
                if (i < other.digits.size()) sum += other.digits[i];
                result.digits.push_back(sum % 10);
                carry = sum / 10;
            }
            result.removeLeadingZeros();
            return result;
        } else {
            if (negative) {
                return other - abs();
            } else {
                return *this - other.abs();
            }
        }
    }

    BigInt operator-(const BigInt& other) const {
        if (negative != other.negative) {
            BigInt result = abs() + other.abs();
            result.negative = negative;
            return result;
        }

        int cmp = compareAbs(other);
        if (cmp == 0) return BigInt(0);

        BigInt result;
        if (cmp > 0) {
            result.negative = negative;
            result.digits = digits;
            int borrow = 0;
            for (size_t i = 0; i < result.digits.size(); ++i) {
                int diff = result.digits[i] - borrow;
                if (i < other.digits.size()) diff -= other.digits[i];
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                result.digits[i] = diff;
            }
        } else {
            result.negative = !negative;
            result.digits = other.digits;
            int borrow = 0;
            for (size_t i = 0; i < result.digits.size(); ++i) {
                int diff = result.digits[i] - borrow;
                if (i < digits.size()) diff -= digits[i];
                if (diff < 0) {
                    diff += 10;
                    borrow = 1;
                } else {
                    borrow = 0;
                }
                result.digits[i] = diff;
            }
        }
        result.removeLeadingZeros();
        return result;
    }

    BigInt operator*(const BigInt& other) const {
        BigInt result;
        result.digits.assign(digits.size() + other.digits.size(), 0);
        result.negative = (negative != other.negative);

        for (size_t i = 0; i < digits.size(); ++i) {
            int carry = 0;
            for (size_t j = 0; j < other.digits.size() || carry; ++j) {
                long long cur = result.digits[i + j] +
                    (long long)digits[i] * (j < other.digits.size() ? other.digits[j] : 0) + carry;
                result.digits[i + j] = cur % 10;
                carry = cur / 10;
            }
        }
        result.removeLeadingZeros();
        return result;
    }

    BigInt operator/(const BigInt& other) const {
        if (other.isZero()) {
            throw std::runtime_error("Division by zero");
        }

        BigInt dividend = abs();
        BigInt divisor = other.abs();

        if (dividend < divisor) {
            if (negative != other.negative) {
                return BigInt(-1);
            }
            return BigInt(0);
        }

        std::string quotientStr;
        BigInt current;

        for (int i = dividend.digits.size() - 1; i >= 0; --i) {
            current.digits.insert(current.digits.begin(), dividend.digits[i]);
            current.removeLeadingZeros();

            int count = 0;
            while (current.compareAbs(divisor) >= 0) {
                current = current - divisor;
                count++;
            }
            quotientStr += char('0' + count);
        }

        if (quotientStr.empty()) quotientStr = "0";
        BigInt result(quotientStr);
        result.negative = (negative != other.negative) && !result.isZero();

        if (result.negative && !(dividend - divisor * result.abs()).isZero()) {
            result = result - BigInt(1);
        }

        return result;
    }

    BigInt operator%(const BigInt& other) const {
        return *this - (*this / other) * other;
    }

    BigInt operator-() const {
        BigInt result = *this;
        if (!isZero()) {
            result.negative = !negative;
        }
        return result;
    }

    double toDouble() const {
        double result = 0;
        double multiplier = 1;
        for (size_t i = 0; i < digits.size(); ++i) {
            result += digits[i] * multiplier;
            multiplier *= 10;
        }
        return negative ? -result : result;
    }
};

#endif
