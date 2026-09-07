// Real Dart: null safety, futures, streams, mixins, extensions.
import 'dart:async';
import 'dart:convert';

mixin Describable {
  String get label;
  String describe() => 'a $label';
}

class Invoice with Describable {
  Invoice({required this.id, required this.total, this.customer});

  final int id;
  final double total;
  final String? customer;

  @override
  String get label => 'invoice #$id';

  factory Invoice.fromJson(Map<String, dynamic> json) => Invoice(
        id: json['id'] as int,
        total: (json['total'] as num).toDouble(),
        customer: json['customer'] as String?,
      );
}

extension InvoiceList on List<Invoice> {
  double get sum => fold(0.0, (running, invoice) => running + invoice.total);
}

Future<List<Invoice>> loadAll(Stream<String> lines) async {
  final invoices = <Invoice>[];
  await for (final line in lines) {
    if (line.trim().isEmpty) continue;
    invoices.add(Invoice.fromJson(jsonDecode(line) as Map<String, dynamic>));
  }
  return invoices;
}
