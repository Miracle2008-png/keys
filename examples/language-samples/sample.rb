# Real Ruby: blocks, symbols, modules, string interpolation.
module Greeting
  DEFAULT = "Hello"

  def self.for(name, greeting: DEFAULT)
    "#{greeting}, #{name}!"
  end
end

class Person
  attr_accessor :name, :age

  def initialize(name, age = 0)
    @name = name
    @age  = age
  end

  def to_s = "#{@name} (#{@age})"
end

people = [Person.new("Ada", 36), Person.new("Alan", 41)]
people.select { |p| p.age > 40 }.each { |p| puts Greeting.for(p.name) }
