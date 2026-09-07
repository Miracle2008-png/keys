// Real Groovy: closures, builders, traits, optional typing.
package com.example.billing

import groovy.transform.CompileStatic
import groovy.transform.ToString

trait Describable {
    abstract BigDecimal getAmount()

    String describe() {
        amount > 10000G ? 'large' : (amount > 0G ? 'standard' : 'empty')
    }
}

@ToString(includeNames = true)
@CompileStatic
class Invoice implements Describable {
    int id
    String customer
    BigDecimal amount = 0G
    String status = 'draft'
}

class InvoiceRepository {
    private final Map<Integer, Invoice> cache = [:]

    Invoice find(int id) {
        cache.computeIfAbsent(id) { key -> new Invoice(id: key) }
    }

    List<Invoice> overdue(List<Invoice> invoices) {
        invoices.findAll { it.status == 'sent' }
                .sort { a, b -> b.amount <=> a.amount }
    }

    def report(List<Invoice> invoices) {
        invoices.each { println "${it.id}: ${it.describe()}" }
    }
}
