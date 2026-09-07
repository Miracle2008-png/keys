# Real Nim: procs, objects, iterators, templates, pragmas.
import std/[strutils, sequtils, tables]

type
  Status* = enum
    Draft, Sent, Paid

  Invoice* = ref object of RootObj
    id*: int
    customer*: string
    amount*: float
    status*: Status

proc newInvoice*(id: int, amount: float = 0.0): Invoice =
  result = Invoice(id: id, amount: amount, status: Draft)

method describe*(self: Invoice): string {.base.} =
  if self.amount > 10_000:
    "large"
  elif self.amount > 0:
    "standard"
  else:
    "empty"

iterator overdue*(invoices: seq[Invoice]): Invoice =
  for invoice in invoices:
    if invoice.status == Sent and invoice.amount > 0:
      yield invoice

template withCache(body: untyped): untyped =
  var cache {.inject.} = initTable[int, Invoice]()
  body

when isMainModule:
  let items = @[newInvoice(1, 42.0), newInvoice(2, 20000.0)]
  echo items.mapIt(it.describe).join(", ")
