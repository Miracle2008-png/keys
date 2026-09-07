// Real Swift: protocols, generics, async/await, property wrappers, enums.
import Foundation

protocol Repository {
    associatedtype Item
    func find(id: Int) async throws -> Item?
}

enum LoadState<T> {
    case idle
    case loading
    case loaded(T)
    case failed(Error)
}

struct Employee: Codable, Identifiable {
    let id: Int
    var name: String
    var salary: Decimal

    var isSenior: Bool { salary > 100_000 }
}

final class EmployeeStore: Repository, @unchecked Sendable {
    private var cache: [Int: Employee] = [:]
    private let lock = NSLock()

    func find(id: Int) async throws -> Employee? {
        lock.lock()
        defer { lock.unlock() }

        if let cached = cache[id] { return cached }

        guard let url = URL(string: "https://example.com/api/\(id)") else {
            return nil
        }
        let (data, _) = try await URLSession.shared.data(from: url)
        return try JSONDecoder().decode(Employee.self, from: data)
    }
}
