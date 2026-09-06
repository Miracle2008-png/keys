//! Real Rust: lifetimes, traits, generics, macros.
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct Cache<'a, K: std::hash::Hash + Eq, V> {
    entries: HashMap<K, V>,
    name: &'a str,
}

impl<'a, K: std::hash::Hash + Eq, V> Cache<'a, K, V> {
    pub fn new(name: &'a str) -> Self {
        Self { entries: HashMap::new(), name }
    }

    pub fn insert(&mut self, key: K, value: V) -> Option<V> {
        self.entries.insert(key, value)
    }
}

fn main() {
    let mut cache: Cache<String, i32> = Cache::new("demo");
    cache.insert("answer".to_string(), 42);
    println!("{:?} has {} entries", cache.name, cache.entries.len());
}
