import { announceCart, api, type Cart } from "../lib/client";

/** The add-to-cart form; posts to /catalog/cart without JS, to the API with JS. */
export default function AddToCart({ productId }: { productId: number }) {
  const submit = async (e: Event) => {
    e.preventDefault();
    const c = await api<Cart>("/api/catalog/cart", { method: "POST", json: { product_id: productId, qty: 1 } });
    announceCart(c.count);
  };
  return (
    <form class="add-form" method="post" action="/catalog/cart" onSubmit={submit}>
      <input type="hidden" name="product_id" value={productId} />
      <input type="hidden" name="qty" value="1" />
      <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
    </form>
  );
}
