import { useState } from "preact/hooks";
import { announceCart, api, money, type Cart } from "../lib/client";

export default function CartView({ initial }: { initial: Cart }) {
  const [cart, setCart] = useState(initial);
  const setQty = async (itemId: number, qty: number) => {
    const c = await api<Cart>(`/api/catalog/cart/${itemId}`, { method: "PATCH", json: { qty } });
    setCart(c);
    announceCart(c.count);
  };
  return (
    <section id="cart">
      {cart.items.length > 0 ? (
        <>
          <table class="cart-table" data-testid="cart-table">
            <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
            <tbody>
              {cart.items.map((it) => (
                <tr key={it.id} data-testid="cart-row">
                  <td><a href={`/catalog/p/${it.product.id}`}>{it.product.name}</a></td>
                  <td>{money(it.product.price_cents)}</td>
                  <td>
                    <form class="qty-form" method="post" action="/catalog/cart" onSubmit={(e) => { e.preventDefault(); setQty(it.id, Number(new FormData(e.currentTarget).get("qty"))); }}>
                      <input type="hidden" name="item_id" value={it.id} />
                      <input type="number" name="qty" min={0} max={99} value={it.qty} data-testid="cart-qty" />
                      <button type="submit" data-testid="cart-update">Güncelle</button>
                    </form>
                  </td>
                  <td>{money(it.qty * it.product.price_cents)}</td>
                  <td>
                    <form method="post" action="/catalog/cart" onSubmit={(e) => { e.preventDefault(); setQty(it.id, 0); }}>
                      <input type="hidden" name="item_id" value={it.id} />
                      <input type="hidden" name="qty" value="0" />
                      <button type="submit" data-testid="cart-remove">Kaldır</button>
                    </form>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
          <p class="cart-total">Toplam: <strong data-testid="cart-total">{money(cart.total_cents)}</strong></p>
        </>
      ) : (
        <p class="empty" data-testid="empty-state">Sepetiniz boş.</p>
      )}
    </section>
  );
}
