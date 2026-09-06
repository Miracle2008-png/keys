// Real Java: generics, streams, records, annotations.
package com.example.demo;

import java.util.List;
import java.util.stream.Collectors;

public record Employee(String name, String dept, double salary) {

    @FunctionalInterface
    interface Filter<T> {
        boolean test(T value);
    }

    public static List<String> highEarners(List<Employee> staff, double threshold) {
        return staff.stream()
                .filter(e -> e.salary() > threshold)
                .map(Employee::name)
                .collect(Collectors.toList());
    }

    public static void main(String[] args) {
        var staff = List.of(new Employee("Ada", "Eng", 120_000));
        System.out.println(highEarners(staff, 100_000.0));
    }
}
