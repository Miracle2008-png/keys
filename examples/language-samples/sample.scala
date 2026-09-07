// Real Scala: case classes, traits, pattern matching, for-comprehensions.
package com.example.billing

import scala.concurrent.{ExecutionContext, Future}
import scala.util.{Failure, Success, Try}

sealed trait LoadState[+A]
case object Loading extends LoadState[Nothing]
final case class Loaded[A](value: A) extends LoadState[A]
final case class Failed(cause: Throwable) extends LoadState[Nothing]

final case class Invoice(id: Int, customer: String, total: BigDecimal)

trait Repository[A] {
  def find(id: Int): Future[Option[A]]
}

class InvoiceRepository(api: Api)(implicit ec: ExecutionContext)
    extends Repository[Invoice] {

  private var cache: Map[Int, Invoice] = Map.empty

  override def find(id: Int): Future[Option[Invoice]] =
    cache.get(id) match {
      case Some(hit) => Future.successful(Some(hit))
      case None =>
        for {
          fetched <- api.fetch(id)
          _ = fetched.foreach(inv => cache += (id -> inv))
        } yield fetched
    }

  def describe(invoice: Invoice): String = invoice.total match {
    case t if t > 10000 => "large"
    case t if t > 0     => "standard"
    case _              => "empty"
  }
}
