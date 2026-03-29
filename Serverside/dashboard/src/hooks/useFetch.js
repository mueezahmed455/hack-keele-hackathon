import { useEffect, useState } from "react";

export default function useFetch(fetchFunction) {
  const [data, setData] = useState(null);

  useEffect(() => {
    fetchFunction().then(setData);
  }, [fetchFunction]);

  return data;
}