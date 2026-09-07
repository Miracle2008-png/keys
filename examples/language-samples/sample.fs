// Real F#: discriminated unions, pattern matching, computation expressions.
module Billing.Invoice

open System
open System.Collections.Generic

type Status =
    | Draft
    | Sent
    | Paid

type Invoice =
    { Id: int
      Customer: string
      Amount: decimal
      Status: Status }

let describe invoice =
    match invoice.Amount with
    | a when a > 10000m -> "large"
    | a when a > 0m -> "standard"
    | _ -> "empty"

let total invoices =
    invoices |> List.sumBy (fun i -> i.Amount)

let overdue invoices =
    invoices
    |> List.filter (fun i -> i.Status = Sent)
    |> List.sortByDescending (fun i -> i.Amount)

let tryFind (cache: Dictionary<int, Invoice>) id =
    match cache.TryGetValue id with
    | true, invoice -> Some invoice
    | false, _ -> None

[<EntryPoint>]
let main argv =
    let sample = { Id = 1; Customer = "Ada"; Amount = 42m; Status = Sent }
    printfn "%s" (describe sample)
    0
