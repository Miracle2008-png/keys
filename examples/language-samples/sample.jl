# Real Julia: multiple dispatch, structs, macros, broadcasting.
module Billing

using Statistics
using Printf

export Invoice, describe, total

@enum Status draft sent paid

struct Invoice
    id::Int
    customer::String
    amount::Float64
    status::Status
end

Invoice(id::Int, amount::Float64) = Invoice(id, "unknown", amount, draft)

function describe(inv::Invoice)::String
    if inv.amount > 10_000
        return "large"
    elseif inv.amount > 0
        return "standard"
    else
        return "empty"
    end
end

total(invoices::Vector{Invoice}) = sum(i -> i.amount, invoices)

function overdue(invoices::Vector{Invoice}; threshold::Float64 = 0.0)
    filtered = filter(i -> i.status == sent && i.amount > threshold, invoices)
    sort(filtered, by = i -> i.amount, rev = true)
end

for method in (:describe, :total)
    @eval export $method
end

end # module
