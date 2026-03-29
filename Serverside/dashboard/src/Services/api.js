export async function fetchData() {
  const res = await fetch("https://api.example.com/data");
  return res.json();
}